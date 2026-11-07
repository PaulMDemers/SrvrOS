# Read-only CPU cache audit for native scanout preparation

The explicit `--intel-cache-capture` profile records CPU0's inherited PAT/MTRR
state alongside the exact Air7,2 display/backing observation. Ordinary startup
and other Intel recipes do not run it. The CPU snapshot/classifier creates no mapping and changes no CPU policy.
The profile reuses the existing temporary read-only backing windows for its
final preservation check. It writes no MSR, GPU entry, invalidate register or
display control.
This is a prerequisite observation, not GPU ownership or DMA authority.

## What it observes

Each of two bounded snapshots gates architectural MSR reads on GenuineIntel,
CPUID PAT/MTRR support, physical width36–52 and normal CR0 caching. It reads
IA32_PAT, MTRRCAP, DEF_TYPE, at most16 variable pairs and the11 architectural
fixed registers when supported. PAT/cap/default and CPU metadata are checked
again at the end. Every read has before/after monotonic checks, with100ms and
49 successful-read caps. Unsupported capacity refuses without indexing a larger
array. Fixed registers are retained but pages below1MiB are not classified.

Snapshots must agree, have positive bounded elapsed time and ordered intervals.
They are non-atomic observations; agreement does not exclude a transient firmware
change or establish future cache ownership. No cache policy is changed on refusal.

The classifier validates active variable masks, alignment, physical width,
reserved fields, legal types and WC capability. It applies defined overlap rules
and rejects undefined overlaps. PAT selection is supplied explicitly as an index
for a hypothetical4KiB leaf; it does not inspect an existing page-table alias.
The memory-type matrix and register interpretation follow Intel's
[SDM volume3A](https://cdrdv2-public.intel.com/812386/253668-sdm-vol-3a.pdf),
sections12.5.2.2/table12-7,12.11 and12.12. In particular, PAT UC-minus can combine
with an MTRR to produce WC; PCD alone is not a strong-UC guarantee. A PAT UC
entry differs from UC-minus. Disabled MTRRs force UC.

## Proposed pages and acceptance limits

Only the existing exact native backing profile plus the observed full34MiB
fence and captured Broadwell CPU signature/width permits three proposed queries:
BAR0+0x101000 (flush-register page), BAR0+0x811000 (GGTT alias for the first page
beyond the full fence), and aperture base+34MiB. Each is evaluated with indices
0,2,3 in both snapshots. These are addresses for cache classification only:
`candidate_mappings=0` means none of these proposed pages are mapped.
No candidate slot/aperture bytes are read and no address range is
imported as managed. The final full inherited1356-PTE/fence revalidation must pass.

The receipt parser independently recomputes types, matches every row and binds
candidate addresses to complete display/backing evidence. CPU-only Q35 results
cannot pass native acceptance. Host producers remain synthetic evidence even
when their protocol fields match this profile.

A native writer still needs a sole boot owner, managed address grant, pinned RAM
with compatible aliases/cache publication, aperture bounds, native GGTT write and
translation-retirement semantics, and exact reader/route restoration. This audit
neither supplies those grants nor changes the [publication contract](scanout-publication.md).

## Reproduction

```sh
make -j2 all test-x86-cache
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL \
  --intel-cache-capture
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

The VM uses an explicit emulated Broadwell CPU to exercise real architectural
MSR reads. Its absent native GPU refuses page binding and aggregate display
acceptance; the saved CPU-only receipt is checked separately. Missing workspace
must halt visibly without reset. Native trials save/unmount and automatically
return after a handled result; persistence failure visibly halts. Preserve Ubuntu
recovery and the exact staged payload, as in the [Ubuntu guide](ubuntu-device-agent-setup.md).

## Air7,2 native observation — 2026-11-04

Source `e9391d64` / release `07df02f62d1074ac`, session
`00065f78ce4bd46c-00`, completes both matching37-read snapshots in46us each,
about11.58 seconds apart. Raw PAT is `0x0000010500070406`, MTRRCAP `0xd0a`,
DEF_TYPE `0xc06`, physical width39. All18 classifications match independent
decoding and exact native display/backing/address binding.

| Proposed physical page | Purpose | Observed MTRR | Effective type at PAT indices0/2/3 |
| --- | --- | --- | --- |
| `0xc0101000` | Flush-register page | UC | UC / UC / UC |
| `0xc0811000` | GGTT alias page beyond the full fence | UC | UC / UC / UC |
| `0xb2200000` | Aperture page beyond the full fence | UC | UC / UC / UC |

No proposed page was mapped or accessed through a new alias; no cache policy or
GPU entry/control was written. Full inherited backing preservation, complete
saved reports, prior-file preservation, offline integrity, recovery and Ubuntu
return checks pass. This accepts the bounded observations only. Actual alias
attributes, private RAM publication, exclusive device/range ownership, native
translation completion and physical tear-free presentation remain separate.
See the [machine record](machines/macbookair7-2.md) for acceptance and retention.
