# CPU backing admission for existing Broadwell scanout

The native v52 [reader/cache preflight](intel-bdw-stolen-scanout.md#native-v52-reader-and-cache-observations-complete)
found two complete6MiB candidates with a hypothetical WC policy at existing PAT5.
Before creating an actual alias, the native provider must also reject OS-owned
frames and existing CPU aliases. This guard implements those CPU prerequisites.
It does not transfer firmware memory, stop GPU/DMA/display readers, reserve an
exclusive hardware interval or allow a pixel store.

## Whole-root absence observer

`vmm_diagnostic_kernel_physical_unmapped` reuses the actual bounded reverse
walker behind `vmm_diagnostic_unique_kernel_aliases`. Existing unique-alias
callers retain their original behavior. The new query accepts one or two
nonoverlapping physical spans of1..4096 pages. Firmware-reserved/MMIO targets
need not belong to ordinary RAM: they are **never dereferenced**. Every page
table followed must still fit the supplied immutable RAM whitelist; no table
may occupy a target span. This does not widen a RAM whitelist to include MMIO.

Caller requirements remain boot CPU0, IRQ exclusion, a sole original kernel
root and four-level paging without PCID/PKU/PKS. CPU physical width, NX/1GiB
support and CR4/EFER bind to the actual context. All reachable present4KiB,
2MiB and1GiB leaves are examined unless the first intersecting leaf is found.
The256-table/131,072-entry ceiling bounds unrelated paths and DAGs as well as
target paths. Cycle, reserved-bit, resource, root, mode or coverage uncertainty
refuses. No allocator, page-table/TLB/cache store or persistent pointer is used.

A complete scan with no intersection succeeds. A present-leaf refusal preserves
the original first raw leaf, its full virtual/physical base and size, candidate
index and scan prefix. It does not resample the failed leaf. Terminal root,
mode and final IF/CR3 guards must pass before that conflict is published;
otherwise context failure wins and the conflict fields stay empty. The prefix
is not an exhaustive alias inventory. The ordinary inventory output remains
untouched on every refusal. Raw leaf flags are observations, not combined
ancestor permissions or coherence/retirement guarantees.

## Native composition

Only the isolated `intel-stolen-v53` / wire6 / configuration7 diagnostic calls
the guard. It runs after both complete translation passes classify the original
layout, while the same CPU display exclusion, kernel root, PCI observer and
source snapshot remain live. A fresh source revalidation brackets the CPU work.
This code does not reconstruct a live owner from a saved report.

For each of the3,072 candidate pages, it counts complete firmware-map coverage
as reserved/framebuffer/other/uncovered and reads the actual PMM reserved tag.
Overlapping carriers or invalid metadata refuse. Unknown/high64-bit type values
remain other types; they are never truncated into reserved. Page-table RAM is
derived separately from usable, ACPI, bootloader and kernel/module RAM entries,
sorted without overlaps; framebuffer/reserved regions cannot become table RAM.
All original resource/root/cache checks remain, with a fresh unchanged CPU cache
context and source check after the whole-root operation.

CPU backing is admitted only when every page has one reserved carrier, every
PMM tag is reserved and the root has no present intersecting CPU leaf. A known
nonreserved frame or present CPU alias is a complete **unavailable** observation,
not successful admission. Other root/table/mode uncertainty is incomplete.
Neither result supplies any of the borrowed-backend's firmware/engine/DMA/cache/
reader/mapping rights. There is no writable alias, target load, physical free,
GPU PTE/command/plane store or ordinary-desktop caller.

The CPU phase consumes the original second pass's remaining2-second budget.
The final fact timestamp includes it; there is no clock restart or deadline
extension. Progress step21 (`CPU-backing`) is saved with no live metadata alias;
the complete route uses24 updates under the existing32-update ceiling.
Safe saved/unmounted outcomes return to Ubuntu; unsafe alias retirement retains
exclusion and halts. The diagnostic is silent and requires no input.

## Receipt and qualification

Wire6 keeps all prior reader/cache/census counters and adds two ordered
`intel-stolen-cpu-span` rows and one `intel-stolen-cpu` row. They retain full
coverage/tag query counts, original physical width/timestamps, RAM range count,
whole-root scan counts and first conflict/root-refusal detail. The collector
checks complete-span geometry, count prefixes, raw leaf encoding/intersection,
physical width against the original CPU snapshots and the CPU-admission result.
Authority stays none and hardware ownership stays false. Historical wire1–5
reports keep their original shape and counts; only v53 can launch a new trial.

`make test-vmm-physical-unmapped` runs the real VMM walker and original unique-
alias regression.3,159 modeled cases cover every candidate page, huge leaves,
upper canonical addresses, adjacent nonoverlap, forbidden table backing,
root/mode/terminal-context drift, bounded DAGs/cycles, untouched failure and
zero stores. All12 compiled weakening controls, fatal ASan/UBSan and native
2KiB frames pass. Actual native-adapter tests cover every PMM query, carrier
and tag refusals, known present-leaf reports and retained uncertainty; these
fixtures do not accept hardware.

## Native result

MacBookAir7,2 source `bc8af8e1`, release `6771b4c6bb8713cc`, v53/wire6/config7,
run `air-stolen-cpu-backing-20261117`, session `session-000660858468bf7b-00`
completes this read-only CPU prerequisite on the actual machine. Each candidate
has1,536 complete reserved firmware-map carriers and1,536 reserved PMM tags;
other/framebuffer/uncovered counts are zero. The39-bit original CPU context and
24 RAM whitelist ranges support a complete110-table /56,320-entry /47,470-leaf
walk with no intersecting present CPU alias. CPU work takes22,924us, including
the original source/cache checks. The two full passes take660,826/705,920us
within their unchanged2-second bounds. All59 metadata aliases retire; fail,
reason, clock, writes and retained counts are zero, with original root4/0/0.

All7 frozen validation jobs,38 focused and197 source/return methods,316 actual
adapter cases,3,159 actual whole-root cases and both exact-stage VM paths passed
before boot. Matching raw bytes/checkpoints, all prior USB contents apart from
expected metadata, offline integrity and fresh recovery/default/consumed-one-shot
guards verify. The run is DIAGNOSED and19 native evidence files are independently
sealed outside build. This accepts the named CPU diagnostic observation only;
no exclusive reservation, writable CPU alias or hardware ownership was granted.

After native evidence, the next provider work must maintain an exclusive
original reservation and firmware/DMA/cache/all-reader ownership. A conflicting
CPU alias must be resolved under a real mapping owner, not blindly removed.
Only then can the [borrow backend](intel-bdw-stolen-borrow.md), actual WC alias/
visibility, first plane transition and latch/old-reader retirement be joined.
