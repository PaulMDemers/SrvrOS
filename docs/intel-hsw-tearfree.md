# MacBookAir6,1 tear-free display bring-up

## Hardware baseline

Ubuntu i915 reports Haswell ULT GT3 (Intel HD Graphics 5000) at `00:02.0`, PCI ID
`8086:0a26` revision 09 and Apple subsystem `106b:011a`. The only connected
output is internal eDP. Its preferred EDID timing is 1366×768 at 60 Hz.
Ubuntu exposes a 4 MiB MMIO BAR and a 256 MiB CPU graphics aperture.
The SrvrOS backing capture observed Pipe A scanning an X-tiled surface from
GGTT address zero, at pitch 5,632 bytes. Its 1,056 image pages sit inside a
16 MiB inherited fence. The entire fence stays protected.
[Air6,1 native backing evidence](macbookair6-1-tiled-backing-audit.md#explicit-linux-model-32-fence-observation-version-3-2026-11-04)
· [Intel i5-4260U specifications](https://www.intel.com/content/www/us/en/products/sku/75030/intel-core-i54260u-processor-3m-cache-up-to-2-70-ghz/specifications.html)

| | Air6,1 Haswell | Air7,2 Broadwell |
| --- | --- | --- |
| Internal panel | 1366×768 eDP | 1440×900 eDP |
| GGTT entry | 32-bit | 64-bit |
| GGTT offset in MMIO BAR | 2 MiB | 8 MiB |
| Inherited fence observed | 16 MiB | 34 MiB |

Haswell stores upper address bits in the low part of the entry, alongside
cache policy. Bit 11 selects cache policy. Graphics address-space and lifetime
contracts can be shared once available on main. The actual bind, translation
invalidation, cache visibility and reader retirement need a Haswell provider.
[Intel Haswell Memory Views, pp. 35–36 and 46](https://kiwitree.net/~lina/intel-gfx-docs/prm/hsw/intel-gfx-prm-osrc-hsw-memory-views.pdf)
· [Linux i915 Haswell encoder and Gen6/7 GGTT path](https://code.googlesource.com/linux/torvalds/linux/%2B/3ca112b71f35dd5d99fc4571a56b5fc6f0c15814/drivers/gpu/drm/i915/gt/intel_ggtt.c)

## Current driver checkpoint

Air7,2 PR301 at `fd722b07` selected boot-scoped takeover that keeps the
inherited primary running. Its one-page bind and rollback run against
modeled hardware. Its native diagnostics separately collect EFI handoff,
reset, queue and mapping-lifetime evidence. Native GGTT writes and tear-free
presentation are still pending. This Air6,1 work uses the same ordering of
milestones and leaves the shared lifetime provider to that PR.
[Air7,2 driver branch](https://github.com/PaulMDemers/SrvrOS/pull/301)
· [Air7,2 roadmap at the reviewed revision](https://github.com/PaulMDemers/SrvrOS/blob/fd722b07/docs/intel-tearfree-roadmap.md)

The first Haswell driver component plans one 4 KiB bind from the existing
two-pass backing capture. It checks the exact inherited primary and both
fence sets, protects the full 16 MiB fence and retains the candidate's raw
DWORD for rollback. It computes the DWORD location and the Gen7 no-cache
encoding for a low-32-bit DMA address. It refuses the observed stolen
and GGTT backing ranges. The test address class is a restriction of this
first experiment. A planned entry carries no reservation or hardware
authority, and the planner performs no I/O. The existing backing capture
shares its Haswell address decoder.
[Planner](../kernel/diagnostics/intel_hsw_ggtt.c)
· [Host capture and planner fixture](../tools/test_hsw_backing.c)

## Candidate observation boot

The opt-in candidate profile runs the original two Haswell backing passes,
then samples slot 4096 at BAR0+`0x204000` twice through one temporary
read-only alias. The first slot outside the 16 MiB fence is useful as an
observation point. A zero entry grants no reservation. The samples are at
least 20 ms apart within a 100 ms window. The Pipe A frame counter must
advance, its programmed and live surface must remain the inherited surface,
and the candidate DWORD and observed display controls must agree. The
candidate alias must retire before a successful report.
[Linux i915 frame counter](https://gbmc.googlesource.com/linux/%2B/5e0c67998152bdb91b056160449ee542b86271a5/drivers/gpu/drm/i915/display/intel_vblank.c)

The report stores conventional PCI Command and PMCSR for IGD and display
HDA, plus PCIe Device Status when the function exposes a PCIe capability.
Without the capability, Transactions Pending is unknown. It also stores the
primary, sprite and cursor controls across the three pipes, and Haswell eDP
PSR control. Firmware/SMM access, VT-d translation state, FBC and engine or
display DMA retirement remain unresolved.
[PCIe Device Status](https://github.com/torvalds/linux/blob/master/include/uapi/linux/pci_regs.h)
· [Haswell PSR register definition](https://code.googlesource.com/linux/torvalds/linux/%2B/4a22709e21c2b1bedf90f68c823daf65d8e6b491/drivers/gpu/drm/i915/i915_reg.h)

Stage the isolated AP probe kernel with `--laptop-probe-volume UUID
--haswell-backing-capture --haswell-tilectl --haswell-linux-fences
--haswell-candidate`. It saves the receipt in the evidence USB's
`srvros-work/Logs` and returns to Ubuntu without a desktop session.
Decode the matching `exit.log` with `tools/hardware_haswell_candidate.py`.

### Native candidate result

Release `b35c7a643be7458e`, source `06d711a2`, saved session
`session-0006614726062673-00`. Both reads returned `0x8ea00001`, a
present mapping to `0x8ea00000` in firmware-reserved stolen memory. That
page differs from all 1,056 sampled primary-image pages. The meaning of
its existing mapping and any other users remain unknown. Keep the slot
protected pending that audit.

Pipe A advanced from frame 934 to 936 in a 21.059 ms observation window.
Programmed and live primary surfaces stayed at GGTT address zero. The
sampled B/C primaries, sprites, cursors and PSR showed disabled controls.
IGD and display HDA both had bus mastering enabled. IGD had no PCIe
capability in its list, so its Transactions Pending state is unknown.
Display HDA reported the bit clear in both samples.

The temporary candidate alias retired with zero GPU writes. Ubuntu returned
with the one-shot consumed, the installed recovery matched its archive,
and the unmounted USB passed the filesystem check. The native result
accepts the read-only observation. It supplies no exclusive binding or
requester retirement proof.

### Slot 4096 access audit

The prior target at `0x8ea00000` is inside the firmware-declared IGD RMRR
`[0x8d800000,0x8fa00000)`. This identifies an explicit firmware DMA
requirement for `00:02.0`. It does not establish the current VT-d mapping or
say which GPU engine is using the address. The active primary image pages
observed so far do not include that address. An address beyond the image can
still be used for display prefetch, compression, or GT work, and a second
GGTT entry can point to the same page. The audio PCI observations also leave
DMA retirement unproved. [Air6,1 DMAR inventory](macbookair6-1-dma-inventory.md#firmware-declaration)
· [Intel VT-d specification, RMRR](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf)

The new `--haswell-access` option extends the candidate recipe. It captures
the pinned DMAR and boot memory catalog in the same boot, samples GGTT entries
4096–4111 twice, and checks Haswell framebuffer compression control, base and
status before and after the candidate bracket. Only one temporary read-only
alias is live at a time. It checks the inherited Pipe A route before and
after the compression samples, requires frame progress and preserves the
original 100 ms candidate limit. The probe does not dereference any target
page or change graphics, PCI or VT-d state.
[i915 compression base programming](https://cos.googlesource.com/third_party/kernel/%2B/3934bbc0446e4cae977a6efe82ce993d37434e53/drivers/gpu/drm/i915/intel_fbc.c)
· [i915 compression register definitions](https://code.googlesource.com/linux/torvalds/linux/%2B/111d0bda8eeb4b54e0c63897b071effbf9fd9251/drivers/gpu/drm/i915/i915_reg.h)

The decoder compares the physical destination to the sampled primary image
pages, nearby GGTT entries, the decoded compression-buffer start and the
DMAR device scopes. A raw FBC base cannot determine the entire buffer
extent. The same target in a nearby entry is positive alias evidence.
Absence of a match in these sixteen entries leaves the rest of the table
unresolved. Live VT-d, command-engine translations, firmware/SMM and
display-reader retirement still require independent, maintained evidence.
Keep slot 4096 protected.

Run the same stage command shown above with `--haswell-access` appended.
Decode the matching `exit.log` with `tools/hardware_haswell_access.py`.

#### Native slot-access result

Release `d0929a7adbcedf69`, source `b1f49e6a`, saved session
`session-00066147aa2af540-00`. Slot 4096 again pointed to `0x8ea00000`.
Entries 4096–4111 mapped 16 consecutive physical pages through
`0x8ea0f000` in both samples. The target did not repeat in that window or
in the 1,056 sampled primary image pages. The same boot's pinned DMAR
records the GPU `00:02.0` as the explicit requester for the reserved DMA
range covering the whole window.

Both framebuffer-compression control, base and status samples were zero.
Both the GPU and display HDA remained bus-master enabled. Pipe A
advanced from frame 935 to 936 in 20.126 ms with its inherited surface
unchanged. The candidate alias and both compression aliases retired with
zero GPU writes. The complete receipt, installed recovery, unmounted USB
filesystem check and consumed GRUB one-shot verified.

The run establishes a stable, populated interval and its declared firmware
DMA scope. Slot 4096 stays protected. The sampled compression state and
local alias check leave live VT-d translations, GT requests, firmware/SMM
access and display-reader retirement unresolved.

### Same-boot VT-d and GPU requester audit

The isolated v9 firmware profile reuses the two-pass Air6,1 graphics
DRHD0 sampler. It checks ECAM, the pinned firmware data, the managed root
and the cache/alias gates exactly as the v8 sampler does. It reads only the
original register list and verifies retirement before later graphics MMIO.
The graphics-specific unit's Global Status reports whether translation is
enabled at each sample. The raw root-address register is retained without
dereferencing it. [Air6,1 VT-d register and caller audit](macbookair6-1-vtd-read-caller-audit.md)
· [Intel VT-d specification, register descriptions](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf)

The boot records IGD PCI Command, PMCSR and optional PCIe Device Status on
both sides of the sampler. After the sampler's files are saved, it requires
the slot 4096 display and alias audit, adds the established read-only GT
core status and RC6 counter samples, and checks the PCI Command, power state
and capability position against the earlier bracket. The decoder binds
`ecam.log`, `window.log` and `exit.log` to the same release and session,
rechecks their order and reports the gap between the VT-d and slot samples.
A clear Translation Enable Status describes that sampled remapping unit.
It cannot prove that the GPU stopped issuing requests. GT power status and
RC6 residency supply no command-queue drain or firmware exclusion proof.
[Haswell status/RC6 history](machines/macbookair6-1.md#ubuntu-rc6-baseline-captured-2026-11-01)
· [Linux i915 GT status reporting](https://gbmc.googlesource.com/linux/%2B/5578b4347bb5d5dfc8eeb8ee2eb8248658707d9b/drivers/gpu/drm/i915/gt/intel_gt_pm_debugfs.c)

The profile uses `--haswell-live-vtd` with the full v8 sampler stage flags.
Decode the matching session directory with
`tools/hardware_haswell_live_vtd.py SESSION_DIR --release RELEASE`. It
preserves the original 100 ms bounds and keeps the inherited framebuffer
active. It issues no GPU or VT-d writes, forcewake, or root-table reads.

#### Native audit refused at the alias census

Source `94d2dd92`, release `c75c79ae778587e3`, and session
`session-000661490d4c5a7f-00` identify the first Air6,1 run. The early
checked ECAM window was adopted once. The pre-user RAM alias census found
42 page tables and reached 4,095 of 4,096 receipt bytes after 13,824 entry
reads in its first raw pass. The next encoded value exceeded the receipt
capacity. The census refused before the second pass and the managed-root
cross-check remained incomplete.

The run issued zero VT-d target reads and zero VT-d writes. It stopped
before the IGD requester bracket, GT sample, and slot 4096 probe. Slot
4096 remains protected on the basis of the earlier access audit. Ubuntu
returned with the one-shot consumed, and the saved snapshots, installed
recovery, and unmounted USB filesystem check verified.

#### Lossless alias receipt encoding

The retry preserves the full ordered page-table graph in each pass. Each
arithmetic run retains its count, initial 64-bit word, and 64-bit stride.
The compact encoding omits only leading zero bytes on the two operands
and uses a tag in the first table address's alignment bits. The decoder
also accepts archived receipts with fixed-width operands. It reconstructs
all entries, including nonpresent words, and rechecks exact pass equality,
graph reachability, RAM provenance and the cache-walk bindings. A receipt
that exceeds 4 KiB still refuses, with the original clock and entry limits.

#### Native retry halted at the sampler deadline

Source `8ff59622` and release `d8bd670a63111f65` reached the first VT-d
sampler pass. The boot requires the pre-user alias gate to pass before
entering the sampler. The photographed pass also reached private frame
allocation, which requires its own fresh alias census to pass. The halt
preceded USB logging, so the exact graph and encoded sizes could not be
verified from a saved receipt.

Pass zero hit the 100 ms `clock/deadline` limit before attempting a
parent link. The last reported firmware checkpoint was
`stable-MCFG-copy`. The screen gives the location of that checkpoint,
while the distribution of time across the pass remains unknown. Neither
pass attempted a VT-d register load. Pass one and the slot 4096 probe
were never reached, and Pipe A continuity has no new evidence.
Ubuntu returned after the halt. The one-shot was consumed, and the
installed recovery and offline USB filesystem check verified.

The next native run records each sampler step's start on the halt screen,
including the census, private frame preparation, and both fresh firmware
gates. It shows the first failing step, its last accepted clock, a rejected
clock sample when applicable, and any later cleanup attempt. Census entry
counts and compact byte counts appear on the same screen. These observations
reuse the guarded clock samples and leave the successful receipt format
and all time and access bounds in place. If the sampler halts, photograph
the complete screen and restart into Ubuntu.

#### Native timing retry halted before unlink

Source `cacc4ac7` and release `0c052b85b197232f` reached all 12 register
reads in the first VT-d pass. The photographed fresh census reported
42 tables and 21,504 entries in each raw pass, encoded in 3,072 bytes
per pass. Its elapsed 81.422 ms took most of the 100 ms lifetime.

Retirement began with 346 µs left in the pass. The clock expired during
the verification before unlink, with one parent-link attempt, zero
unlink attempts and zero freed frames. The sampler retained ownership
until restart. The second pass and slot 4096 audit never started. The
photo is the evidence for this partial run because the halt preceded
USB logging. The Mac returned to Ubuntu, and the one-shot, recovery
payload and offline USB check verified.

#### Census performance retry

The sampler resolves its active pass state once per pass and uses recent
row indices to accelerate searches of the validated RAM catalog. A row
lookup rechecks full-page coverage, type and excluded ranges on every
request. A miss runs the bounded search. The live HHDM walk, per-entry
clock reads, root checks, raw comparisons and retirement verification
remain in force. The same 100 ms pass deadline, inter-pass gap and 4 KiB
census receipt cap apply. Both passes must retire before slot 4096 is read.

#### Native read-only audit completed

Source `1b582efe`, release `2291c93c536fbe3d` and session
`session-0006614a1336e89e-00` passed the joined receipt decoder. Both VT-d
passes verified retirement and released their claims at 99.581 and
98.729 ms. They performed 24 register reads and zero writes. The fresh
42-table censuses took 74.887 and 74.595 ms and retained 3,088 bytes per
raw pass. The 97.429 ms gap between read batches met the existing bound.

Translation Enable Status was clear in both samples of graphics DRHD0.
IGD bus mastering remained enabled at both PCI endpoints. Slot 4096
returned `0x8ea00001` twice and still maps `0x8ea00000` in IGD's declared
RMRR. Pipe A advanced two frames in 20.128 ms on the inherited surface.
The candidate alias and both compression aliases retired with zero GPU
writes. The exit snapshot, recovery and offline USB filesystem check
verified after Ubuntu returned with the one-shot consumed.

Slot 4096 stays protected. Requester drain, firmware/SMM exclusion and a
maintained owner remain open. The smallest observed retirement margin was
419 µs.

### Requester drain and firmware exclusion

The next v9 release adds two read-only observations to the existing
slot 4096 bracket while Pipe A advances. The conventional PCI walk checks
Advanced Features capability `0x13` on IGD and display HDA. The receipt
contains the raw capability header and control/status for each endpoint.
Transactions Pending is unknown if the capability is absent or lacks TP
support. At the two ends of the slot bracket, the pinned Haswell BSP reads
its 32-bit SMI counter. The host decodes the modulo-32-bit delta. The unit
also exposes an SMI counter through Ubuntu's `msr` PMU. [PCI register definitions](https://raw.githubusercontent.com/torvalds/linux/v6.12/include/uapi/linux/pci_regs.h) · [Linux SMI counter sampling](https://raw.githubusercontent.com/torvalds/linux/v6.12/tools/power/x86/turbostat/turbostat.c)

A pending bit or advancing SMI counter supplies direct evidence of
activity within its scope. Clear endpoint samples and a zero CPU0 delta
leave intervening traffic and firmware access on other CPUs or agents
unresolved. EFI boot-services exit alone grants no continuing exclusion
of runtime firmware or SMM. A later firmware-exclusion claim needs an
explicit trusted platform handoff with a maintained hold, or hardware
enforcement for all relevant agents. [UEFI boot-services handoff](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html)

Graphics DRHD0 advertises both read and write drain support. The captured
ECAP locates the Haswell IOTLB command register at `0x108` relative to
DRHD0. Broadwell PR301 exercises an ordered drain under its own held
requester lifetime. Haswell still needs separate admission, register
state/interlock qualification, command completion and retirement, under
a hold that prevents new submissions. The remapper's drain cannot alone
retire GT caches, the display reader or firmware. Engine queue state would
need an independently qualified awake access path. The ring registers
are in the i915 forcewake range, so this trial leaves them unread.
[Intel VT-d specification](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf) · [Intel Haswell command registers](https://kiwitree.net/~lina/intel-gfx-docs/prm/hsw/intel-gfx-prm-osrc-hsw-commandreference-registers_0.pdf) · [i915 forcewake access](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/intel_uncore.c)

The receipt grants no requester drain or firmware exclusion authority.
All VT-d, PCI and GPU controls and the slot 4096 entry remain unchanged.

#### Native requester observation completed

Source `d7ecead3` / release `1f1ba1d9afa3649c` passed the joined decoder on
session `session-0006614b141c63d8-00`. Both VT-d passes retired in 99.819
and 98.733 ms with 24 total reads and zero writes. The slot observation
started 3.601 seconds after that sampler retired.

IGD exposed Advanced Features capability `0xa4` with Transactions Pending
support. Its pending bit was clear at both slot endpoints, with bus mastering
enabled. Display HDA exposed no Advanced Features capability. CPU0's SMI
count was 13 at both ends of the 20.129 ms slot bracket. Pipe A advanced two
frames on the inherited surface. Slot 4096 still mapped `0x8ea00000`, and
all temporary aliases retired. These observations leave requester drain
and firmware exclusion open. Slot 4096 stays protected. Ubuntu returned
with the one-shot consumed, and the saved receipt, recovery and offline
USB check verified.

#### Separate drain-state boot test

The v10 `--haswell-drain-state` profile uses the same guarded lifetimes and
replaces the v9 register sequence for this boot. Each pass starts with GSTS,
VER and ECAP-low. It requires GSTS zero, legacy VER `0x10`, and the previously
observed ECAP-low `0x00f0101a` before proceeding to CCMD, FSTS and IOTLB.
Both 64-bit command registers are read high/low/high. FSTS and GSTS repeat
within the batch. The second pass must match the first and both mappings
must retire. The receipt uses the existing wire layout with a distinct boot
token and register allowlist. [Intel VT-d register descriptions](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf)

Each pass reads twelve DWORDs. The first release bounded each lifetime to
100 ms. The retry allows 110 ms for v10 only, with the same 100 ms census
cap and 20–100 ms between read batches. The joined decoder
reports raw command and fault status and distinguishes a pending command
from a clear bit. It binds that sample to the later inherited-surface
observation. Slot 4096 stays protected. The profile issues no invalidation,
fault acknowledgement, requester inhibition or firmware control.

#### Native drain-state trial halted during retirement

Source `caedb228` / release `05fbf4639efa4821` completed the first pass's
12 register reads. The halt photograph shows `CCMD=0x0800000000000000`,
`FSTS=0x00000000` and `IOTLB=0x0200000000000000`. The context and IOTLB
pending bits were clear in that batch. The fresh 42-table census took
75.370 ms and fit in 3,072 bytes per raw pass.

Retirement began with 7.199 ms remaining. The 100 ms deadline expired
after the unlink attempt began and before either frame was freed. Unlink
completion and full retirement remain unverified. Pass two and the slot
4096 audit were skipped. The halt preceded USB logging, so
this is a photographed first-pass observation. Ubuntu returned with the
one-shot consumed, and recovery and the offline USB check verified.

The retry records the last retirement step and guarded timestamps for the
linked verification, parent unlink, readback, CPU invalidation, unlinked
verification, frame release and final guard. The first census already needs
most of the lifetime. The extra 10 ms keeps the full graph rechecks and
guarded retirement in place. Acceptance requires both retirements, the
existing identical-read and graph checks, and a subsequent read-only
slot 4096 observation with Pipe A active.

#### Native drain-state retry completed

Source `8c824c61` / release `ac08271ebcde3224` saved session
`session-0006614cd5690d31-00`. The two VT-d mappings retired and released
their claims in 99.209 ms and 98.302 ms under the 110 ms limit. Each census
covered 42 tables and used 3,084 bytes per raw pass. The read batches were
97.001 ms apart and returned identical values across 24 reads and zero
target writes. CCMD and IOTLB showed no pending command, and FSTS was zero
in both batches.

The joined receipt verified the subsequent read-only slot 4096 audit.
The slot still held `0x8ea00001` at both endpoints, and Pipe A advanced
from frame 1014 to 1015 on the inherited surface over 21.390 ms. The
temporary audit aliases retired. IGD pending status was clear at both
endpoints and the CPU0 SMI count stayed at 13. Requester drain and firmware
exclusion remain unproved. Ubuntu returned with the one-shot consumed, and
the archived exit snapshot, installed recovery and offline USB check verified.

### Read-only disjoint boot-slot observation

The separate v6 Haswell backing profile samples slot 16385 at GPU address
`0x04001000`, outside the inherited 16 MiB fence and distinct from slot
4096. The Air7,2 branch chose the same GPU address for its own reset-held
checkpoint. On Haswell, the 32-bit entry sits at BAR0+`0x210004`. The
temporary alias permits that DWORD only. Two reads must match at least
20 ms apart, with the inherited Pipe A surface unchanged, its frame
counter advancing, and the alias retired inside the existing 100 ms
limit. [i915 GGTT BAR layout](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L1063-L1097)

The receipt records whether the entry is present. Even a stable zero
grants no range, backing memory or authority to bind. A later bind needs
a maintained requester and firmware exclusion contract. Both slot
4096 and the original framebuffer stay protected in this trial.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-candidate
--haswell-boot-slot`. Decode the matched `exit.log` with
`tools/hardware_haswell_candidate.py --boot-slot`.

#### Native slot 16385 observation

Source `2e02607c` / release `0b596a06d71138d6` saved session
`session-0006614d04e5536b-00`. The read-only alias retired in the
20.063 ms bracket with zero writes. Pipe A advanced from frame 1091
to 1093 while the programmed and live surface stayed at GPU address
zero.

Both slot 16385 reads returned `0x8da00001`. The decoded backing
address, `0x8da00000`, is the start of reserved stolen memory and
matches page zero of both inherited Pipe A backing vectors. Slot
16385 is populated with an alias of an active framebuffer page.
Keep it protected. Ubuntu returned with the one-shot consumed,
and the exit snapshot, installed recovery and offline USB check verified.

### Bounded GGTT candidate survey

The v7 Haswell backing profile surveys four fixed 1,024-entry ranges at
GGTT slots 8192–9215, 24576–25599, 40960–41983 and 57344–58367. These
cover four 4 MiB regions of GPU address space starting at 32, 96, 160 and
224 MiB. The inherited fence and slots 4096 and 16385 stay outside the
survey. The temporary CPU alias only maps the current GGTT table page
read-only and uncached. The survey never dereferences a PTE's backing
address. [i915 GGTT BAR layout](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L1063-L1097)

Both passes read the same 4,096 DWORDs, separated by at least 20 ms and
bounded together by 100 ms. The adapter checks the inherited Pipe A
route, surface, display wells and other plane controls at each table
page. The frame counter must advance, all eight temporary mappings must
retire, and the second pass must match every raw word. Drift, unavailable
reads, timeout or uncertain retirement leave no candidate shortlist.
The receipt contains one verified vector and the accounting and hashes
from both passes. The host reports exact-zero slots, present entries and
aliases of the captured Pipe A image and reserved stolen memory. An
exact-zero slot is only a candidate for a separate ownership check.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-survey`. Decode the
matched `exit.log` with `tools/hardware_haswell_survey.py`.

#### Native GGTT survey result

Source `8e120584` / release `e8cd36755ebbe4cf` saved session
`session-0006614d796bfb16-00`. Both 4,096-entry passes agreed, with a
20.002 ms inter-pass gap. All eight temporary aliases retired within
the 22.119 ms survey. Pipe A advanced from frame 1002 to 1003 with
the inherited surface preserved. The verified receipt records zero writes.

Every sampled PTE was present. Slot 8192 and all 3,072 entries in the
other three ranges mapped the first inherited framebuffer page at
`0x8da00000`. The remaining 1,023 entries in the first range were
populated and outside the captured stolen-memory range. The exact-zero
shortlist was empty. Ubuntu returned with the one-shot consumed, and
the exit snapshot, installed recovery and offline USB check verified.

#### Survey of the remaining aperture

The v8 Haswell backing profile surveys the remaining 57,342 eligible
entries in fourteen four-page batches. The plan covers slots from 4096
through 65535, with the four v7 ranges and the two protected slots removed.
It never reads the inherited fence. For table pages that contain slots
4096 and 16385, the temporary mapping is read-only and the exact DWORDs
are masked in both the loop and the MMIO accessor. Their neighbors
remain in scope. The backing memory behind the observed entries is
never dereferenced.

Each batch compares two passes word for word and retains the 20 ms
inter-pass gap and 100 ms lifetime. Each batch checks the inherited
Pipe A route, frame progress and retirement of all eight temporary
mappings before the next batch starts. A failed batch stops the sweep.
The receipt fits in the exit log using per-batch scope, timing,
accounting, matching hashes, presence counts and a bounded exact-zero
shortlist. The kernel checks raw equality before producing those counts.
The host checks the complete batch schedule and recorded totals. An
exact-zero candidate still needs a separate ownership check.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-sweep`. Decode the
matched `exit.log` with `tools/hardware_haswell_sweep.py`.

##### Native remainder sweep result

Source `458ccd16` / release `9d79622438a1cd2b` saved session
`session-0006614ee4e3dac3-00`. All fourteen batches completed in
310.308 ms. Individual batches took 22.129–22.154 ms. Pipe A advanced
one or two frames in every batch while the inherited surface stayed
active. The two passes agreed in each batch, all 112 temporary mappings
retired, and the receipt records zero writes.

Every one of the remaining 57,342 entries was present. Of these,
53,245 matched the first inherited framebuffer page and 57,340
pointed into the captured stolen-memory range. Across this sweep and
the four earlier rows, all 61,438 eligible entries were present.
The exact-zero shortlist was empty. Ubuntu returned with the one-shot
consumed, and the exit snapshot, installed recovery and offline USB
check verified.

### Populated range takeover with Pipe A active

The Air6,1 surveys found all 61,438 eligible entries in the 256 MiB CPU
aperture present. An entry that points to the first framebuffer page
reveals its backing address and leaves its users unknown. The sweep
also reports entries outside the captured stolen-memory range, with
their ownership unresolved. The archived backing receipt reports a
2 MiB GGTT table. The CPU aperture survey covers its first 256 MiB of
GPU address space. A GPU-only address would require a separate
display-addressability and verification path. For this checkpoint,
keep the full 16 MiB fence, slots 4096 and 16385, and their backing
protected. [Intel Haswell memory views, pp. 34–35 and 46](https://cdrdv2-public.intel.com/690966/intel-gfx-prm-osrc-hsw-memory-views.pdf)

#### What Air7,2 establishes

PR301's live path keeps the inherited framebuffer pinned across a
GT-only reset, memory maintenance and requester checks. On the
native Air7,2, slot 16385 still pointed to the first framebuffer page
after reset and memory maintenance, before bus mastering reopened.
The modeled one-page writer holds one driver range loan through
scratch publication, bind and rollback. Its first-publication path
requires an exact-zero PTE inside a previously granted range.
The software registry accounts for competing leases once a hardware
owner supplies that grant. [Air7,2 live range qualification](https://github.com/PaulMDemers/SrvrOS/blob/0fc30fd5/docs/intel-tearfree-roadmap.md#driver-owned-range-qualification)
· [Range registry](https://github.com/PaulMDemers/SrvrOS/blob/0fc30fd5/kernel/include/srvros/scanout_space.h)

PR301 at `0fc30fd5` also exercises a separate panel-dark recovery
path with memory decoding on and bus mastering off. Native recovery
restored the framebuffer and both CRCs, but its latest census
refused the first alias check and read zero entries. That path
interrupts scanout and falls outside the live Pipe A contract.
[Air7,2 native status](https://github.com/PaulMDemers/SrvrOS/blob/0fc30fd5/docs/machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

#### Haswell admission and rollback conditions

1. **Pin the inherited display under one boot owner.** Bind the owner
   to the exact device, boot epoch, BARs, original route, programmed and
   live surface addresses, fence, backing pages and fetch padding.
   Serialize SrvrOS graphics and CPU aperture clients before admission.
   Keep Pipe A fetching the original surface through the entire later
   transaction. Reserve a separate GPU address extent, including any
   required guard pages, and keep its old backing alive. The future
   destination must be independently pinned OS RAM. Linux i915 pins
   the initial plane during its own device takeover. [i915 initial-plane pinning](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_plane_initial.c#L212-L231)
2. **Obtain and maintain hardware exclusion for that exact range.**
   Establish who can submit or modify translations before acquiring the
   range and what prevents them from doing so through retirement.
   Account for each enabled engine and requester, posted transactions
   and prefetch, display readers outside the pinned surface, and
   firmware or SMM. Clear VT-d command bits, clear PCI Transactions
   Pending and an unchanged SMI count remain endpoint evidence. GOP
   describes the inherited framebuffer and `ExitBootServices` ends boot
   services. The range grant would need a separately established
   firmware handoff contract or enforcement that covers all relevant
   writers and readers. No such grant is available on this Air6,1.
   [UEFI boot services handoff](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html#efi-boot-services-exitbootservices)
3. **Qualify the Haswell hardware boundaries separately.** Before a
   future write trial, establish an awake register-access contract and
   the exact engine inventory. Qualify any GT-only reset under a maintained
   owner with fresh scope, completion and live-display evidence.
   i915 selects its Gen6-style reset path for Haswell and Gen8 reset
   preparation for Broadwell. Forcewake acknowledgement permits
   register access and reset acknowledgement attests to the targeted
   reset. Outgoing DMA, cache, prefetch and reader retirement each need
   separate evidence. Air6,1 has no native evidence for live scanout
   under a function-wide bus-mastering hold. Keep IGD bus mastering,
   forcewake and reset controls untouched in the read-only checkpoint.
   [i915 reset dispatch](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c#L630-L651)
   · [i915 Haswell forcewake setup](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L2106-L2109)
4. **Use one future range loan for replacement and restoration.** Once
   a hardware grant exists, snapshot and recheck the original populated
   DWORD under the hold. The current Haswell planner can encode a
   proposed private page and retain the rollback word. A future writer
   needs an owned alias, an ordered Gen7 invalidate and a translation
   witness before any aperture access. A first bind should target one
   page and verify the private observation, then restore and verify the
   exact original word and reader retirement before returning the loan
   or releasing the private backing. i915's Gen6/7 invalidate uses a
   flush-control write and posting read under the uncore lock. This supplies an ordering
   reference. If the hold or rollback becomes uncertain, pin the loan
   and both backings for controlled recovery. [Haswell planner](../kernel/diagnostics/intel_hsw_ggtt.c)
   · [i915 Gen6/7 invalidation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L177-L184)

#### Read-only ownership refusal boot

The v9 Haswell backing profile verifies refusal for slots 24576–24591,
starting at GPU offset `0x06000000` and BAR0+`0x218000`. The preceding
survey observed this region populated with aliases of the inherited
framebuffer. The new boot compares two 16-entry samples, at least 20 ms
apart and within the existing 100 ms window. Pipe A must advance on the
same inherited surface and each temporary read-only alias must retire.
The MMIO reader is limited to those 16 entries. The inherited fence
and slots 4096 and 16385 stay protected. Backing addresses are compared
with the saved image vector and are never dereferenced by the probe.

A verified refusal requires all entries present and both raw samples
matching. It reports how many addresses alias the inherited image, then
records the unavailable range grant, requester hold and awake engine
state and the unverified DMA drain and firmware exclusion. A changed,
non-present or incomplete sample cannot produce a verified refusal.
The host checks the raw vector, hashes, image alias count, frame
progress and both retirements. The profile has no range grant or hardware
control path.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-ownership-refusal`.
Decode the matched `exit.log` with `tools/hardware_haswell_ownership.py`.

##### Native ownership refusal result

Source `4b8fcf94` / release `a725799216dbd870` saved session
`session-0006615005417a06-00`. Both passes read all sixteen entries
and returned the same raw word, `0x8da00001`, for every entry. All
sixteen decoded addresses matched the first inherited framebuffer page.
Pipe A advanced from frame 1074 to 1076 in 25.836 ms on the inherited
surface. Both temporary aliases retired, with zero hardware writes.

The receipt verified refusal. The range grant, requester hold and awake
engine state were unavailable, and DMA drain and firmware exclusion
remain unverified. Ubuntu returned with the one-shot consumed. The
exit snapshot, installed recovery and offline USB check verified.

#### Read-only engine and requester prerequisites

The Haswell i915 profile lists render, video decode, blitter and video
enhancement engines. Their ring registers require forcewake access.
The i915 Haswell wake path checks the acknowledgement and waits for
GT thread C0 to avoid spurious zero reads. A later SrvrOS engine sample
needs a continuously held awake reference for this exact device and
domain, with the required thread state and a checked release. The
comparison supplies register and access rules for the proposed provider.
[i915 Haswell engine inventory](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/i915_pci.c#L364-L387)
· [i915 ring registers](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_engine_regs.h#L9-L25)
· [i915 forcewake ranges and generation dispatch](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L2321-L2331)
· [i915 GT thread wait](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L349-L365)

The v10 Haswell backing profile records two bounded observations while
Pipe A scans the inherited surface. Each endpoint reads the Haswell
forcewake ACK at BAR0+`0x130044` through a temporary alias admitting
only that DWORD. It also reads GT thread and core status through the
existing graphics status page, IGD PCI Command, PMCSR and Advanced
Features status, and CPU0's SMI counter. The endpoints are at least
20 ms apart and both aliases must retire within the 100 ms observation.
The backing capture verifies the inherited fence and surface. The
check skips the protected slots and issues no ring reads, wake requests
or hardware control writes.
[i915 Haswell ACK and GT status definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_gt_regs.h#L1379-L1411)
· [i915 Haswell wake-domain setup](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L2106-L2109)

A set ACK and sampled thread C0 describe those endpoints. A clear
Transactions Pending bit only covers the sampled PCI capability. A
verified hold needs one live owner and epoch tied to the exact GPU
range, with new submissions inhibited, earlier traffic and translations
retired, and every other requester, including firmware, excluded
through release. Broadwell PR301 is developing that contract for its
own occupied-slot loan while keeping Pipe A live. The Haswell receipt
records `awake_owner=unavailable` and `requester_hold=unverified`
for every combination of raw endpoint values.
[PCI Advanced Features definitions](https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/pci_regs.h#L331-L340)
· [Air7,2 occupied-slot scope](https://github.com/PaulMDemers/SrvrOS/blob/cb578981/docs/machines/macbookair7-2-current.md)

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-engine-prereqs`.
Decode `exit.log` with `tools/hardware_haswell_engine_prereqs.py`.

#### Native engine and requester observation

The MacBookAir6,1 receipt on source `0220fb23` and release
`5cb53191c13342ba` verified both endpoints in 20.067 ms. Pipe A stayed
active on the inherited surface and advanced from frame 924 to 925.
Both temporary ACK aliases retired. The probe recorded zero hardware
writes, zero ring reads and zero reads of protected slots 4096 and 16385.

Both endpoints recorded ACK `0x00000000`, GT thread status
`0x00030303` and GT core status `0x11800303`. The thread sample did
not show C0. IGD was in D0 with bus mastering enabled and the PCI
Advanced Features Transactions Pending bit clear at both reads. CPU0's
SMI count stayed at 13. Awake ownership was unavailable, and requester
hold, DMA drain and firmware exclusion remain unverified.

Ubuntu returned with the one-shot consumed. The exit snapshot, installed
recovery and offline USB check verified.

#### Bounded forcewake acquire and release

The v11 isolated Air6,1 recipe exercises one kernel render forcewake
request. The exact device, inherited backing, fence and Pipe A route
must qualify before the request. The probe starts with an all-clear
ACK at BAR0+`0x130044`. Only the kernel request bit at BAR0+`0xa188`
is writable through a separate supervisor cache-disabled alias. The
probe writes `0x00010001` to acquire and `0x00010000` to release.
Other requests, engine registers, and GGTT slots 4096 and 16385 are
outside the probe. The ACK alias stays read-only.
[i915 Haswell wake domain](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L2106-L2109)
· [i915 masked request and release](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L117-L119)

The ACK polls each have a 10 ms budget. The test checks the GT thread
status bits for C0 within 5 ms of ACK, checks the original display
route under the request, and reads ACK again before clearing.
It requires at most 20 ms from request to the clear attempt. After any
issued request it attempts one clear even on acquisition, clock or
display failure. If release is uncertain, both aliases stay reserved
until reboot. Success requires clear ACK, verified retirement of both
aliases and Pipe A frame progress after a 20 ms post-release gap,
all inside the existing 100 ms window. This verifies the transient
kernel request. Requester hold, DMA drain and firmware exclusion
require separate evidence.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-forcewake`. The
unattended profile saves the result to the USB workspace and returns
to Ubuntu. Decode the exit snapshot with
`tools/hardware_haswell_forcewake.py`.

#### Native forcewake result

The MacBookAir6,1 receipt verified the kernel forcewake cycle on source
`d8f8ec68`, release `d0683f5c9453e017`. ACK read `0` before the
request, set to `1` in 55 µs and stayed set across the GT thread
read. The thread status was `0xc0000000`, with the low three C0
status bits clear 4 µs after ACK. ACK returned to `0` in 4 µs
after the clear attempt.

Pipe A stayed on the inherited framebuffer and advanced from frame 924
to 926. Both aliases retired and the full observation took 21.211 ms.
The receipt recorded two masked stores, zero engine-ring reads and zero
reads of protected slots 4096 and 16385. The exit snapshot, installed
recovery and offline USB check verified after Ubuntu returned with the
one-shot consumed. Requester hold, DMA drain and firmware exclusion
remain unverified.

#### Engine idle and requester status under forcewake

The v12 Air6,1 recipe reuses the native-tested forcewake cycle. Before
asserting the kernel bit it prepares four read-only aliases for render,
video decode, blitter and video enhancement. Two passes sample only
ring HEAD, TAIL, CTL, MI_MODE and ACTHD. Each pass checks ACK and GT
thread C0 before and after its reads. The first pass and the second
pass are separated by at least 250 µs, with both inside a 4 ms
sampling budget and the existing 20 ms request-to-clear limit.
[Haswell engine inventory](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/i915_pci.c#L364-L386)
· [i915 ring-idle predicates](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_engine_cs.c#L1736-L1757)

The decoder reports an engine idle at the samples when all five words
match across the passes, masked HEAD and TAIL agree, the ring is free
of wait flags and MI_MODE reports idle. It retains changed, running,
and unavailable samples for diagnosis. Both passes also record PCI
Command, PMCSR, PCIe Device Status and Advanced Features status where
present for IGD and display HDA, plus CPU0's SMI count. Pending,
clear, changed configuration and unavailable indications are reported
per requester. Clear status or idle rings provide evidence at the
sampled endpoints. A requester drain still needs proof of submission
exclusion, completion of earlier transactions and translations, and
firmware exclusion.

The only hardware stores are the masked forcewake acquire and clear.
On release failure all aliases remain reserved until reboot. A
qualified receipt verifies release, retirement of all six aliases and
Pipe A frame progress on the same inherited surface within 100 ms.
Slots 4096 and 16385 stay protected.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-engine-drain`.
The unattended profile saves the result to USB and returns to Ubuntu.
Decode the exit snapshot with `tools/hardware_haswell_forcewake.py`.

#### Native held-engine result

On source `b9121084` and release `f53f55d9ba361ef2`, the first pass
completed in 46 µs with ACK set and GT thread C0 at both ends. All
four rings reported MI_MODE idle and matching masked HEAD and TAIL.
Render, video decode and video enhancement had disabled rings. The
enabled blitter reported HEAD, TAIL and ACTHD at `0x970`. IGD
Advanced Features Transactions Pending and display HDA PCIe
Transactions Pending were clear in this pass, with PCI bus mastering
enabled for both devices. CPU0's SMI count was 13.

The next recorded clock was 10.688 ms after the first pass ended.
The 4 ms sampling budget expired before the second pass began.
The receipt has the initial Pipe A frame 927 and no closing frame
sample. Two-pass engine idle and requester status, Pipe A continuity
across the trial, requester drain and firmware exclusion remain
unverified. The cause of the clock gap is unconfirmed.

Forcewake ACK cleared 5 µs after the release attempt. All six aliases
retired, and the receipt recorded zero reads of protected slots 4096
and 16385. The exit snapshot, installed recovery and offline USB check
verified after Ubuntu returned with the one-shot consumed.

#### Held-engine timing retry

The first pass took 46 µs. The failure landed in the wait between
passes, where the loop only polls the ACPI PM clock. The 10.688 ms
gap is consistent with the native 100 Hz LAPIC tick dispatching
other work. The original receipt has no interrupt or SMI bracket
across the gap, so its cause remains unconfirmed.

The retry saves CPU0's interrupt flag and masks local maskable
interrupts for the two samples and inter-pass wait. It restores
the prior flag before forcewake release, alias retirement and log
writes, including after a sampling fault. The 250 µs minimum gap,
4 ms sampling budget, 20 ms hold bound and 100 ms full window still
use the ACPI PM clock. The timing receipt records timer ticks,
scheduler preemptions, CPU0 SMI counts, the longest clock gap and
the stage and reason the sampler stopped.

The host fixture delivers a pending timer interruption as soon as
the prior interrupt flag is restored. Both passes finish before
delivery. A separate injected 10.688 ms elapsed gap while interrupts
are masked still stops before pass two and runs verified release.

The native retry on `69837e0d` / `0e13ef71a2ab8dad` verified both
passes in 335 µs against the 4 ms budget. The passes took 46 and 37 µs,
with 251 µs between them. CPU0's timer ticks stayed at 67, preemptions
at 33 and SMI count at 13. The largest clock gap was 11 µs during
pass one. The earlier timing failure's cause remains unconfirmed.

ACK was set and GT thread C0 was observed at both ends of each pass.
All four engines reported idle at both samples. IGD Advanced Features
and display HDA PCIe Transactions Pending were clear at both samples,
with bus mastering enabled for both devices. Requester drain and
firmware exclusion remain unverified.

Pipe A advanced from frame 925 to 926 on the inherited framebuffer.
ACK cleared and all six aliases retired within the 20.472 ms overall
trial. The protected slots were untouched. The exit snapshot, installed
recovery and offline USB check verified after Ubuntu returned.

#### Requester drain and firmware handoff audit

The native held-engine result supplies idle ring snapshots and clear
pending bits. The next proof needs one owner for the exact device,
range and boot epoch. The owner must block new SrvrOS engine
submissions, account for display HDA and remaining readers, and
hold the inherited Pipe A surface, fence and fetch padding for the
whole interval. That hold must survive the completion checks and
retirement. Air7,2 PR301's separate ownership gate also reports an
explicit firmware transfer as its first unmet prerequisite.
[Air7,2 ownership gate status](https://github.com/PaulMDemers/SrvrOS/pull/301)

The previously observed graphics DRHD0 advertises read and write
drain support. On a platform where the owner controls the active
remapper, the local drain proof must identify the requester scope,
invalidate applicable translations, and verify completion of
the requested read/write drain. VT-d completion covers the
specified memory traffic. Endpoint-cached translations, GT
translation and prefetch state, peer traffic and display-reader
retirement require their own checks. The earlier Haswell DRHD0
observations showed translation disabled. This audit uses the
existing state and issues no remapper commands.
[Intel VT-d, section 6.5.4](https://cdrdv2-public.intel.com/831418/vt-directed-io-spec.pdf)

The v13 boot audit pairs both engine and PCI requester samples
with Lynx Point-LP SMI routing observations. It verifies the
LPC bridge identity, PCI PMBASE and ACPI decode against the
calibrated FADT timer at `0x1808`, then reads `GEN_PMCON_1` and the
PM I/O `SMI_EN` and `SMI_STS` words. The receipt records global
SMI enable and its lock at each valid endpoint, alongside the
existing CPU0 SMI count. An enabled route documents that source
of firmware entry even when the count is steady. A disabled
route describes the sampled PCH setting. A maintained firmware
transfer or hardware exclusion covering all relevant agents is
still required. UEFI boot-services exit leaves runtime firmware
interfaces available.
[Intel 8 Series PCH datasheet, sections 12.1 and 12.8](https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/8-series-chipset-pch-datasheet.pdf)
· [UEFI runtime services](https://uefi.org/specs/UEFI/2.11/08_Services_Runtime_Services.html)

SMI enables, locks and status are read only. The only hardware
stores are the two previously qualified masked forcewake stores.
The existing 4 ms sample, 20 ms hold and 100 ms full limits
apply. Pipe A stays on the inherited surface and the probe skips
slots 4096 and 16385. The audit reports requester drain and
firmware exclusion as unverified.

Stage with `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-exclusion-audit`.
The trial saves its receipt to USB and returns to Ubuntu. Decode
the matched `exit.log` with `tools/hardware_haswell_forcewake.py`.

##### Native requester and SMI observations

The v13 audit on `6ed8f878` / `39155cff051f9264` completed both passes
in 354 µs. Both PCH samples read `GEN_PMCON_1=0x1ae4`,
`SMI_EN=0x33` and `SMI_STS=0x100`. Global SMI delivery was enabled
and its lock bit was clear at both samples. CPU0's SMI count stayed
at 13 across the sampling window.

All four engines reported idle. IGD Advanced Features and display
HDA PCIe Transactions Pending were clear at both samples. Both
devices had bus mastering enabled. Requester drain and firmware
exclusion remain unverified.

Pipe A advanced from frame 918 to 919 on the inherited framebuffer.
Forcewake released, all six aliases retired, and the protected slots
4096 and 16385 stayed untouched. The exit snapshot, installed recovery
and offline USB check verified after Ubuntu returned.

#### Maintained requester hold and firmware handoff

The next admission boundary needs a single owner for IGD, display HDA,
the exact proposed GGTT range and the boot epoch. The owner pins
Pipe A's inherited surface and any fetch padding, closes admission
to new command and copy work, and waits for work already admitted
to retire. It must also account for display HDA and any remaining
readers. Completion evidence must cover outstanding requests and
translations for the range, with admission kept closed until the
operation and rollback state are retired. The Air6,1 endpoint
receipts leave IGD and display HDA bus mastering enabled. A scoped
hold that keeps the inherited scanout live needs its own native
qualification before any range grant.
[Intel VT-d drain scope](https://cdrdv2-public.intel.com/831418/vt-directed-io-spec.pdf)

Firmware handoff needs an independently verified transfer for the
same device, range and epoch. A provider must cover firmware and
SMM readers and writers for the life of the requester hold and
provide a checked release. An instrumented firmware implementation
with a reviewed transfer contract is one way to establish that
evidence. Air7,2 PR301's range work also keeps its firmware-transfer
prerequisite separate. The original Apple firmware on this Air6,1
has no validated transfer provider in SrvrOS. Boot-services exit
ends boot services and leaves runtime firmware interfaces available.
[Air7,2 ownership work](https://github.com/PaulMDemers/SrvrOS/pull/301)
· [UEFI runtime services](https://uefi.org/specs/UEFI/2.11/08_Services_Runtime_Services.html)

The v14 boot audit records IGD's ASLS pointer and SWSCI
configuration in both held-engine passes, alongside the validated
SMI route and requester observations. It classifies the nominal
8 KiB OpRegion extent from the boot memory map and checks both
PCI values again in each pass. The receipt records whether the
pointer and SCI-select or trigger bits were stable and whether a
single memory descriptor covered the extent. The OpRegion supplies
a firmware-driver communication interface. A live pointer and stable
configuration describe its availability. The recipe maps and reads
no OpRegion payload, sends no firmware message, and grants no
requester hold or firmware handoff.
[i915 ASLS and SWSCI definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_pci_config.h#L94-L99)
· [i915 OpRegion discovery](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_opregion.c#L808-L880)

Both protected GGTT slots and the inherited framebuffer stay outside
any change. The established forcewake acquire and clear are the only
hardware writes, with Pipe A continuity and the 4 ms, 20 ms and
100 ms limits verified as before. Stage with
`--laptop-probe-volume UUID --haswell-backing-capture --haswell-tilectl
--haswell-linux-fences --haswell-handoff-audit`. Collect the matched
`exit.log` and decode it with `tools/hardware_haswell_forcewake.py`.

#### Native OpRegion and requester observations

Air6,1 release `5e395aeb45b7ba0a` from source `35e76b8b` completed
both passes in 364 µs in session `session-00066153863f2898-00`.
Both samples recorded stable `ASLS=0x8cd18190`, with the nominal
8 KiB extent covered by one ACPI NVS descriptor. `SWSCI=0x8000`
selected SCI with the trigger clear. The audit accessed only the
PCI configuration and boot memory-map metadata for these observations.

All four engines reported idle. IGD Advanced Features and display
HDA PCIe Transactions Pending were clear at both samples with bus
mastering enabled. PCH global SMI remained enabled with the lock
clear. The sampled CPU0 SMI count stayed at 13. These observations
leave the maintained requester hold and firmware handoff unavailable.

Pipe A advanced from frame 918 to 920 on the inherited surface.
The probe left slots 4096 and 16385 untouched. Forcewake ACK cleared
in 4 µs after release and all six temporary aliases retired. The
installed release, final snapshot, archived replay, installed recovery
and offline USB check verified after the one-shot returned to Ubuntu.

#### Requester admission hold prototype

The isolated v15 boot binds one software owner to the exact IGD and display
HDA functions and populated GGTT slots 24576–24591. The owner exists before
the first admission check and stays active through both bounded read-only
passes and verified alias retirement. The probe checks admission again after
retirement, while the hold is still active. Both checks refuse authority
because the Air6,1 has no verified firmware handoff provider. Retirement
failure retains the guard until reset. Reentry and out-of-range mappings
remain blocked.

The inherited Pipe A route and surface stay live. Both passes must preserve
the populated words, verify a frame advance and finish under the existing
100 ms limit. The guard covers SrvrOS diagnostic admission. Hardware drain
and firmware exclusion still need independent evidence. The profile issues
zero GPU or PCI configuration stores and leaves slots 4096 and 16385
untouched. It saves a receipt and returns to Ubuntu automatically.

Stage using `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-requester-hold`. Collect
the matching `exit.log` and decode it with `tools/hardware_haswell_ownership.py`.
Require a verified refusal, two retirement proofs, positive Pipe A frame
advance, and two admission refusals bracketing the reads. The receipt must
show `firmware_handoff=unavailable`, `hardware_hold=unverified` and no authority.

#### Native requester admission hold result

Air6,1 release `43a82a6c8897b493` from source `b65bfd4c` completed in
session `session-000661587e4511c3-00`. The boot-local guard refused GGTT
ownership before the first pass and again after the second alias retired.
Both passes read the same 16 populated entries in slots 24576–24591.
All 16 aliased the first page of the inherited framebuffer. Each pass
retired its temporary mapping, and the guard retired after 21.782 ms.

Pipe A stayed on the inherited surface and advanced from frame 920 to
921. The probe left slots 4096 and 16385 untouched and issued zero GPU
or PCI configuration stores. Firmware handoff remained unavailable and
the hardware requester hold unverified. The installed release, final
snapshot, archived replay, recovery and offline USB check verified after
the one-shot returned to Ubuntu.

#### Reset-free requester drain audit

Haswell documents a per-engine parser-stop and synchronizing-flush sequence.
`MI_MODE` exposes the parser's Stop Ring and Ring Idle bits. `INSTPM`
exposes the synchronizing-flush request and engine-specific controls. The
render memory-sync control is bit 8, while the other three use bit 6.
The blitter's bit 9 is reserved, and its `MI_MODE` bypass bit can suppress
writes associated with a flush. Treat the Intel video-decode programming
note that names the blitter mode register as ambiguous; require a checked
video engine sequence before attempting that request. The command stream
reference also describes engine-local sync status and hardware status
page writes. A future trial needs fresh completion evidence with a known
status destination. An idle parser and a clear request bit can both exist
before a request has ever been issued.
[Haswell register reference, pp. 40–41, 49–51, 302–303, 309–310, 393–394, 399–400 and 425–433](https://cdrdv2-public.intel.com/690873/intel-gfx-prm-osrc-hsw-commandreference-registers_0.pdf)
· [Haswell command stream reference](https://cdrdv2-public.intel.com/690970/intel-gfx-prm-osrc-hsw-command-stream-programming_1.pdf)

The Air7,2 Broadwell verifier binds a maintained traffic block to fresh
engine and L3 acknowledgements. Haswell needs its own equivalent authority
and completion scope. The Haswell per-engine controls leave shared-cache
completion, translations, display fetches, display HDA and firmware outside
their proof. i915 also invalidates GGTT translations through a separate
register operation. Before any active experiment, bind command ownership,
firmware exclusion, the inherited Pipe A backing and the status destinations
for the whole hold. Then qualify an engine stop, fresh completion and release
without relying on old completion status or altering a live scanout entry.
[Air7,2 ownership work](https://github.com/PaulMDemers/SrvrOS/pull/301)
· [i915 GGTT invalidation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L2446-L2460)

The isolated v16 audit reads the four Haswell `INSTPM` DWORDs in each of
the two held-engine samples. The decoder binds them to `MI_MODE` and
reports Stop Ring, Ring Idle, Sync Flush Enable and the blitter bypass
bit. All-ones observations remain unavailable. The inherited PCI and
OpRegion evidence stays in the same bracket. Only the previously qualified
masked forcewake acquire and clear stores run. Pipe A continues scanning
and the test keeps slots 4096 and 16385 untouched. The 4 ms sample, 20 ms
hold and 100 ms full limits carry forward. The receipt grants no requester
drain or GGTT authority.

Stage using `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-reset-free-audit`.
The probe saves its receipt and returns to Ubuntu. Decode the matching
`exit.log` with `tools/hardware_haswell_forcewake.py`.

##### Native reset-free audit result

Air6,1 release `a71fe4aaa95f6592` from source `05931e32` completed both
held-engine samples in 374 µs in session `session-000661598206ec1a-00`.
`INSTPM` read `0x6080` on render and `0` on video decode, blitter and
video enhancement at both endpoints. All four `MI_MODE` values were
`0x200`. The rings reported idle, Stop Ring and Sync Flush Enable were
clear, and the blitter bypass bit was clear. Both requester pending
indications were clear at the samples. Requester drain remains unverified
and firmware handoff unavailable.

Pipe A advanced from frame 920 to 921 on the inherited framebuffer.
The probe left slots 4096 and 16385 untouched and issued no ring-stop,
flush, or GPU reset requests. Forcewake ACK cleared in 8 µs after release,
and all six aliases retired. The installed image, exit snapshot, archived
replay, recovery and offline USB check verified after Ubuntu returned.

#### Reset-free blitter hold prototype

The prototype exercises the blitter only. An eventual native provider must
hold a live ticket for the exact device and boot epoch, keep the original
Pipe A surface and status destination pinned, and exclude firmware and prior
or competing commands and their delayed completions. A ticket must remain
valid through the final reopen read. The verifier checks this contract at
the operation and sample boundaries.

The verifier requires an idle, open parser with fence-write bypass disabled
and both the flush request and sync completion clear. It stops the parser
and checks the clear baseline again with Stop Ring and Ring Idle asserted.
It then issues one modeled sync flush. The request must clear and the
blitter sync completion bit must become set under the same ticket. An
uncertain operation, lost ticket, stale completion or deadline after stop
retains ownership. A verified completion permits one reopen and a final
read confirming Stop Ring is clear. The whole verifier uses a 4 ms and
65,536-sample bound. It has no reset, GGTT store, or range-grant operation.
[Haswell command stream reference, blitter Sync Status](https://cdrdv2-public.intel.com/690970/intel-gfx-prm-osrc-hsw-command-stream-programming_1.pdf#page=25)

The isolated v17 native boot runs the validated v16 observations and checks
the same verifier's admission. The current Air6,1 firmware and completion
destination lack a verified native provider. The receipt requires refusal
with zero stop, flush and reopen attempts and no completion claimed. The
only hardware stores are the qualified masked forcewake acquire and clear.
Pipe A stays on the inherited surface and slots 4096 and 16385 stay outside
the probe. This stages native refusal while the host fixture exercises the
modeled hold, fresh completion, and verified reopen.

Stage using `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-engine-hold`. Decode the
matching `exit.log` with `tools/hardware_haswell_forcewake.py`.

##### Native blitter hold refusal

Air6,1 release `b1fc397952af8f7a` from source `3efdf3fd` completed in
session `session-0006615a20659429-00`. Both held-engine samples completed
in 375 µs with all eight `INSTPM` reads. Admission returned
`state=refused reason=no-provider` with zero polls and zero ring-stop,
flush or reopen attempts. Active completion and requester drain remain
unverified.

Pipe A advanced from frame 921 to 923 on the inherited framebuffer. The
probe left GGTT slots 4096 and 16385 untouched. Forcewake ACK cleared in
8 µs after release and all six aliases retired. The installed image, exit
snapshot, archived replay, recovery and offline USB check verified after
the one-shot returned to Ubuntu.

#### Native blitter completion provider audit

Haswell uses the Gen7 blitter status page address register at `0x4280`.
i915 programs it with a pinned GGTT status-page address. Intel documents
blitter Sync Status at bit 24 and requires the corresponding HWSTAM and
IMR bits to permit hardware status-page writes. This makes the inherited
address and masks useful evidence for selecting a completion source. The
address can name a protected or inherited slot, and a zero register is
ambiguous. A permissive mask still leaves the destination, DMA mapping and
prior completion state to be proven.
[i915 Gen7 register definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_gt_regs.h#L2711-L2738)
· [i915 status-page setup](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ring_submission.c#L2283-L2398)
· [Haswell blitter interrupt definition](https://cdrdv2-public.intel.com/690970/intel-gfx-prm-osrc-hsw-command-stream-programming_1.pdf#page=25)
· [Haswell register reference](https://cdrdv2-public.intel.com/690873/intel-gfx-prm-osrc-hsw-commandreference-registers_0.pdf#page=424)

A future provider needs enforceable exclusion of firmware and competing
blitter commands, ownership of the exact completion page and translation,
and a way to establish a fresh, unambiguous completion with the same ticket.
It must keep Pipe A's framebuffer pinned and independently verify restoration
of the owned completion route. The original Apple boot path has no verified
handoff for those rights. The independent Broadwell work cannot issue a
Haswell ticket. [Air7,2 ownership work](https://github.com/PaulMDemers/SrvrOS/pull/301)

The isolated v18 boot records the blitter PGA, HWSTAM and IMR in both held
passes and retires the extra read-only PGA register alias. The verifier
classifies the raw address against the inherited image, fence and protected
slots without reading its GGTT entry or mapping the status page. It preserves
the raw masks, including all-ones values. The same native engine hold refusal
remains required. The only stores are the established forcewake acquire and
clear. Pipe A stays active, slots 4096 and 16385 stay untouched, and the 4 ms,
20 ms and 100 ms bounds carry forward.

Stage using `--laptop-probe-volume UUID --haswell-backing-capture
--haswell-tilectl --haswell-linux-fences --haswell-blitter-provider`. Decode
the matching `exit.log` with `tools/hardware_haswell_forcewake.py`.

After the private bind, qualify complete pinned scanout buffers, confirm
synchronous primary-plane latch at vblank and independently retire readers
before reusing the old front buffer. CPU rendering can supply the first two
surfaces. Keep the inherited framebuffer as the fallback through physical
validation.
