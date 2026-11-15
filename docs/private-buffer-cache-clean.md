# Private-buffer CPU cache writeback prerequisite

The separate v35 Air7,2 diagnostic prepares both original 1,356-page allocations
for future CPU-to-device visibility work. This adds CPU writeback of private RAM;
it does not flush a GPU cache, retire DMA or display readers, invalidate GPU
translations, import an interval or clear a hardware ownership obligation.
Ordinary desktop startup and older diagnostic images do not execute this path.

Before the original GPU clocks start, retain both original allocator holds and
exclude IRQ-driven CPU work. The holds already block CPU borrows, mapping and
release. This trusted original-holder maintenance operation grants no CPU borrow
and writes no buffer data. Check their identity, layout, lack of exposure, frame tags, requested
CPU mappings, uniform WB policy and full present-alias inventory of the sole
active root. Require the captured Intel signature, CPUID CLFSH feature and its
64-byte line size. Check the actual physical-to-virtual span before any address
can reach CLFLUSH. The CPU feature bits must agree with the fresh cache context.

For each buffer, execute MFENCE, then for every page revalidate the exact
original frame, current translation and WB policy, and issue a local INVLPG for
that admitted virtual page before one CLFLUSH for every 64-byte line of every
allocated page including padding, then MFENCE after the full buffer. The two buffers contain 173,568
lines in total and 2,712 local page invalidations. These target the current-root
addresses; the architectural instruction can evict additional CPU TLB entries.
It is not a GPU/IOMMU invalidation. Clocks and pure original-hold guards surround
each page. All
maintenance and its opening/closing validation fit a separate 100 ms preparation
window; the existing GPU 100/300/400/500 ms deadlines are unchanged. A deadline
failure cannot accept the operation. A mandatory non-addressed cleanup fence
follows any partial failure after an attempted line, without another buffer
access or replay. Lost completion or closing hold/root/cache/CPU identity keeps
RAM pinned and causes the existing visible uncertainty halt. Safely rejected
CPU-only work can return through the existing private-RAM cleanup path.

The completed record remains bound to the original spans/root and is required
by the held child and pure final parent-return gate. No CPU borrow or buffer
store occurs between cleaning and the original diagnostic retirement. Existing
full patterns are read again only after retirement. CPU speculation can refill
cache lines: completion does not assert permanent invalid residency or GPU
coherence. Other CPU roots, stale aliases outside these targets, GPU/IOMMU mappings, outstanding
DMA/prefetch and affected firmware/display readers still need their own proof.

The optional `intel-private-clean: v=2 b=...` row is 72 canonical little-endian
bytes: completed buffers, failure reason, completed main-path fences and line
bytes (`4*u8`); begin/end (`2*u64`, with end carrying the failed clock sample on clock refusal); CPUID signature/EBX/EDX (`3*u32`); then two
physical base/page count/completed line count groups (`u64,u32,u32`), followed
by two completed page-invalidation counts (`2*u32`). The abandoned unqualified
version1 format is not accepted as physical CPU writeback evidence. Failure
reasons are input1, identity2, clock3, fence4, line5 enclosing-preparation6 and target-page refresh7.
Only two complete buffers, four fences, exactly 1,356 invalidations per span,
exact original spans, admitted CPU data,
positive bounded clocks before the outer lifetime and a complete original audit
can claim the new CPU writeback scope. Historical rows without this packet gain
no writeback acceptance. Native decoding also compares the CPU data with the
independent original cache snapshot. All 255 hardware obligations and zero
assurances/managed GPU pages remain.

Intel's [instruction reference, CLFLUSH and INVLPG](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf)
describes the feature/line enumeration, writeback and ordering rules, including
speculative refills. The explicit fences also cover the older conservative
ordering model. Host fixtures exercise the actual bounded core with callbacks;
they do not execute native instructions on fabricated physical addresses.
Native object checks must verify the real operand and fence instructions and
their isolation to v35 before exact-stage VM and physical qualification.

Native acceptance is pending. The [Air unit record](machines/macbookair7-2.md#active-scope-private-buffer-cpu-cache-writeback)
owns the precise tested source, qualification and later physical result.
