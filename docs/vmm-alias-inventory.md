# Whole-root private-buffer CPU alias inventory

This diagnostic searches the complete reachable page-table tree of the sole
active kernel root for mappings of at most two private physical spans. It is
separate from the requested-alias observer: both remain required in the v35
source audit. CPU0, existing IRQ exclusion and an immutable captured RAM map are
caller preconditions. No page-table pointer escapes the call.

The walker checks four-level paging, current CPU physical width and NX/1GiB
support, with fresh CR4/EFER context checks. PCID, LA57, PKU and PKS modes refuse.
Every slot in every reachable table path is read; every present leaf is decoded.
Non-present slots do not translate. A legal shared subtree is visited for each
virtual path; an ancestor cycle refuses. Unsupported present bits, physical-width
or large-leaf alignment errors, missing coverage and unexpected virtual aliases
refuse. Equal cache attributes do not excuse an unexpected alias. A large leaf
contributes only its intersection with each requested span, never neighboring
pages or a donated whole leaf. Matching counts bind independently to the existing
requested-alias leaf/page counts.

Table reads must fall wholly inside captured Usable, Bootloader Reclaimable or
Executable/Modules RAM. These are HHDM-mapped types in the pinned Limine protocol
(base revision3 and newer); framebuffer/MMIO, ACPI and unsupported memory types
are excluded. Page tables overlapping the requested backing refuse. The caller
sorts at most256 captured RAM regions before the original outer clock; overlap,
malformed maps and capacity exhaustion refuse. The VMM call independently checks
the ranges and reads at most256 logical table paths (131,072 slots). Refusal and
partial scans leave the result untouched. No allocation, PTE/cache/TLB operation,
clock callback, register write or target-buffer access occurs.

Both original-root/registry/IRQ guards enclose each scan. Terminal mode/IRQ/CR3
reads follow the final pointer conversion, so no conversion hook follows those
last observations. This remains a point-in-time CPU0 page-table observation:
it does not inspect stale TLB entries, alternate/SMP roots, GPU/IOMMU aliases,
DMA, cache coherence, firmware activity or display-reader retirement. It grants
no ownership or completion assurance.

## Held-source integration and wire data

The v35 audit performs one whole-root scan for both original private buffers in
each of its two existing samples, under the original source epoch, requester
inhibit and all-five engine hold. Existing requested mappings, PAT/MTRR checks,
fresh full guards, original clocks and all deadlines remain. The pure final
parent-return gate requires both completed inventories and original span counts.
Failure keeps the existing ownership chain pinned; it cannot authorize replay,
restore or release through an unknown state.

The optional `intel-alias-census: v=1 b=...` row contains60 canonical little-endian
bytes: completed sample count (`u8`), failure reason (`u8`), zero reserved (`u16`),
then two groups of seven `u32` values: tables, slots read, present leaves,
matching leaves for buffer0/buffer1, covered pages for buffer0/buffer1. Counts
bind to the original requested mappings. Both successful samples must agree on
topology counts. Partial/unread suffixes cannot claim completion. Historical
receipts without this row retain their earlier requested-alias scope. This
receipt supplies no hardware ownership flags; all255 obligations stay open.

The repeated progress banner is shortened while retaining automatic/no-input,
saved-return and unsafe-halt instructions. The existing64KiB tail budget and
reserved output must qualify again. The original100/300/400/500ms limits stay
fixed; the prior6,608us margin is one observation, not promised capacity.

## Acceptance

Implementation and host tests are under development in the
[Air7,2 unit record](machines/macbookair7-2.md#active-whole-root-cpu-alias-inventory).
No physical whole-root inventory or new DMA/cache/reader acceptance is claimed.
