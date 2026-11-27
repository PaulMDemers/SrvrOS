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

### Private bind prerequisites

A later write trial requires an owner that maintains exclusion from
admission through a posted DWORD store, Gen7 translation completion and
exact rollback. Linux i915's Gen6/7 path follows the DWORD store with a
flush-control write and posting read under the uncore lock. That is a
reference for ordering; SrvrOS still needs its own native admission and
completion evidence. [Linux i915 Gen6/7 GGTT invalidation](https://code.googlesource.com/linux/torvalds/linux/%2B/3ca112b71f35dd5d99fc4571a56b5fc6f0c15814/drivers/gpu/drm/i915/gt/intel_ggtt.c#192)

After the private bind, qualify complete pinned scanout buffers, confirm
synchronous primary-plane latch at vblank and independently retire readers
before reusing the old front buffer. CPU rendering can supply the first two
surfaces. Keep the inherited framebuffer as the fallback through physical
validation.
