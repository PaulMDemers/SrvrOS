# Air7,2 read-only DMA-remapping observation

The separate `--intel-dma-capture` profile extends the exact Broadwell inherited
[display/backing capture](macbookair7-2-display-presentation.md). It observes the
GPU-specific VT-d unit, not a new display owner or DMA mapper. Ordinary startup,
VirtIO and the independent Haswell diagnostic do not enter this path. No native
GGTT range is granted; see the [reservation contract](native-scanout-reservations.md).

## Admission and access

Require the existing exact firmware/GPU/PCH profile, complete recognized backing,
retired mappings, kernel CR3 and reference clock. The unit's captured 136-byte
DMAR table must have a valid length/checksum, revision1, address width39, flags3,
the exact three-entry layout/scopes and full raw FNV hash
`0x722775ad66626863`. The GPU endpoint is segment0/bus0/device2/function0;
its DRHD is `0xfed90000`. The separate include-all unit `0xfed91000` is never
mapped. The GPU RMRR `[0x8b800000,0x90000000)` remains reserved. Synthetic table
fixtures have a different header/hash and cannot pass native admission.

The register page requires exactly one enclosing firmware RESERVED entry with
no overlapping RAM/framebuffer/other entry. Missing or ambiguous classification
refuses before MMIO, even if Ubuntu's table lists the address. This is a new
unit-specific gate, not an inference from another Mac's resources.

Reuse the existing backing window only while retired. Map one supervisor,
read-only, NX, uncached page directly in the kernel address space, outside the
global mapping registry. Refuse an existing mapping, wrong CR3 or sticky mapping
fault; recheck its physical identity before every read. An uncertain unmap
retains the mapping state and prevents reuse until reboot.

The only offsets read are VER `0x00`, CAP `0x08/0x0c`, ECAP `0x10/0x14`,
GSTS `0x1c`, and RTADDR `0x20/0x24`. Each64-bit pair uses high/low/high reads;
GSTS brackets each sample. Do not read command/fault acknowledgement registers,
follow RTADDR, allocate translation tables, disable remapping, issue invalidation
or touch the other unit. The capture API has no write callback.

The reference is Intel's [VT-d Architecture Specification, revision4.1,
sections11.4.1–11.4.5](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf).
GSTS bit31 reports translation-enable status. RTADDR contains the programmed
root address/mode; its read value is not proof that a command loaded that table.
Neither an observed disabled bit nor a root pointer establishes DMA authority.

## Bounds and receipts

Two12-read samples must be stable, each within100ms, separated by20–100ms.
A one-million-iteration gap bound also stops a frozen clock. Recheck the exact
DMAR bytes, PCI/plane/well guards and resource classification before/after each
sample; all unit mappings must retire. Afterward reread the entire inherited
PTE/fence backing and compare it to the original snapshot. A changed guard,
torn pair, read/mapping failure, clock rollback, deadline or failed retirement
prevents observation completion. No error is interpreted as a disabled unit.

Version1 receipts include the complete canonical base64 DMAR table, independent
hash/resource endpoints, two raw samples and mapping/preservation outcome.
The host decoder recomputes the table hash/checksum and exact scopes, validates
every row/count/time/state and requires complete display/backing prerequisites.
It always returns DMA authority, hardware ownership and physical tear-free as
false. The observed TES bit is evidence about these two brackets only.

The report, including admission diagnostics, is bounded below4KiB; the actual backing formatter plus this receipt
and16KiB logging reserve fit the existing64KiB tail. The guest shows progress,
saves reports and automatically returns after clean unmount on success or a
handled refusal/failure. Failed saving/unmount still halts visibly. No human
input is required. Q35 must refuse before MMIO and preserve save/return behavior;
a missing workspace must halt without an automatic reset.

```sh
make -j2 all test-intel-dma
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL \
  --intel-dma-capture
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

Collect only the matching native session and release; validate `intel_dma_observe`,
`intel_display_capture` and `intel_display_backing` before ordinary preservation,
offline integrity and recovery checks. Refusal, missing rows or a VM fixture is
not native acceptance. The next writer still needs explicit DMA/GGTT/firmware/
route/restore authority and correct padded scanout layout. The shadow allocation
is1350 visible pages, not the inherited X-tiled1356-page storage extent.

## Admission diagnostics

Additional version1 `admission`, `memory` and up to six `region` rows preserve
the same access gates. `checked` and `passed` are bit masks; `failed` names the
first failed bit: profile1, exact DMAR2, unmapped window4, mapping retirement8,
kernel CR3 16, firmware resource32, reference clock64. Preflight checks profile
then table; each guard checks window, retirement, CR3, profile, table, resource,
clock in that order. Unreached checks remain clear. `guards` counts invocations.

A resource check validates at most4096 firmware entries and retains up to four
intersections with `[0xfed90000,0xfed91000)` plus the nearest disjoint entry
before and after it. Region relation0 is overlap,1 before,2 after. `shown` and
`omitted` count stored/dropped intersections; `conflicts` counts non-reserved
intersections. `valid` means the entire catalog passed structural validation,
not that access was admitted. A checked valid catalog with zero intersections
proves missing coverage; an unchecked catalog says nothing about classification.
Malformed catalogs refuse and can contain only the validated prefix's evidence.
No register read is needed to collect these rows.

The decoder validates pairing, check order/masks, row bounds, region arithmetic
and relationships. Historical version1 receipts without these paired diagnostics
remain readable. Diagnostics never grant authority or make an incomplete
observation complete. The new diagnostics require a fresh native trial; the
older refused session cannot establish their result.

## Native trial result

Source `c7c3548d`, release `5f6ec95399aa9dcd`, passes full local host/core/
profile/build checks, pure-core ASan/UBSan, strict collectors and the exact-stage
Q35 refusal/save/unmount/reset and missing-workspace halt checks. Separate
Haswell refusal and ordinary VirtIO presentation regressions also pass.

Native session `00065f6af281f582-00` saved both logs and returned automatically.
Display/backing observations completed, but the DMA observer's initial guard
refused with status2 (`DMA_ACCESS`), zero maps/reads/writes and no retained
mapping. The raw DMAR identity matches; the aggregate receipt correctly says
`completed=NO`. The resource row is zero, which does not distinguish an
unreached check from missing/conflicting firmware classification. No translation
state was sampled. Native DMA observation remains **unaccepted**.

The next diagnostic must identify which guard failed and report relevant
firmware memory-map classifications before any MMIO. Keep the existing admission
gates until the actual evidence and resource contract are reviewed. Prior-file
preservation, offline integrity, independent recovery and consumed one-shot
checks passed. No reboot is armed. The [unit record](machines/macbookair7-2.md)
owns physical acceptance; no DMA authority or physical page flip is implied.
