# Private command-arena GPU mapping transaction

`intel_command_mapping` joins the [CPU command-memory owner](intel-command-memory.md)
to an **already transferred** four-page GPU interval through the existing
[transfer](scanout-transfer.md), address-space and publication cores. This is
host-qualified software. No native image links it and no native provider supplies
the required interval or ownership witnesses. The accepted Air7,2 CPU arena is
still CPU-owned storage; all255 whole-device obligations remain open.

## Admission and registration

The original zero-initialized mapping object must live in static/heap storage.
Its caller supplies the original returned CPU arena, active transfer and exact
token. The CPU lease root/epoch/provider, transfer source epoch and callback
provider/clock must agree. The transfer must already own exactly four pages,
all five handoff assurances, zero open obligations and the complete original
64bit entry vector. Scratch, absent, stable or populated entries cannot establish
that authority. The provider must keep inherited physical targets pinned.

Before any provider callback, the transaction registers itself on the CPU RAM
hold and reserves all four GPU pages. Registration blocks CPU close/return and
the native parent's existing return gate. Only verified mapping retirement may
remove it. The CPU hold does not become GPU ownership merely by registration.

Fresh independent provider proofs bind the exact mapping object, CPU arena,
transfer, root, epoch, physical backing and GPU interval. All five requirements
must hold: transferred interval, private RAM, CPU aliases, DMA/cache ownership
and reader ownership. Every proof must have a timestamp within the surrounding
source-clock samples. A descriptor echo is not a native proof.

The callback table/context, CPU lease and allocator/mapping table, hold serial,
transfer descriptor/vector/exclusion, range serial, publication metadata and
four actual entries are checked across callbacks, including clocks and pauses.
Copied owners, reentry, changed sources and overlapping output storage refuse.
This is a CPU0-serialized trusted-kernel contract, not an arbitrary-memory-write
security boundary or an SMP/IOMMU ownership mechanism.

## Publication and restoration

The existing Broadwell codec plans one4KiB mapping for each actual CPU arena
page. It uses a present GGTT entry and aligned QWord access at
`GTTMMADR + 0x800000 + 8*index`, following Intel's
[Broadwell memory views](https://www.x.org/docs/intel/BDW/intel-gfx-prm-osrc-bdw-vol05-memory_views_0.pdf)
and pinned Linuxv6.12's
[GGTT encoding/publication](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c).
The native access adapter would still have to validate the real register mapping
and hardware ownership immediately before each access.

1. Capture all four original entries through the publication core and compare
   them exactly with the transfer's owned vector.
2. Publish all four replacements with known issuance, perform actual ordered
   posted-write and translation/cache completion, then verify readback.
3. Run one registered bounded child. Its address lookup is data only; it grants
   no permission to initialize an executable LRC, change RAM or submit a ring.
4. Drain readers, restore all four exact originals, complete again and verify
   readback and reader retirement before releasing the range and RAM dependent.

A successful transaction has20 actual entry reads, eight known stores, two
completion operations and reader checks equal to restoration polls plus one.
Both reader checks matter: a retired publication cannot release backing after a
new reader appears during restoration. Originals are opaque64bit values; tests
restore targets above4GiB exactly even though private command backing is below4GiB.
An ordered CPU fence, MMIO readback or isolated System Agent invalidation alone
cannot implement the provider's completion obligation.

Known pending readers permit another bounded restoration poll, up to16 attempts.
Unknown ownership must fail the independent ownership proof. Any started failure
retains the RAM registration, range and transfer exclusion. Partial/uncertain
stores, lost proofs, changed callbacks and failed completion cannot trigger
optimistic rollback, force cleanup or retry. There is no allocator-release
callback in this transaction. Later CPU cleanup still requires original-parent
retirement and preservation of all16KiB of zero storage.

## Clocks and validation

Every nested source-clock sample is accounted against the original preparation
origin:100ms by default,150ms under the existing selected diagnostic policy.
GT300ms, HDA400ms and outer500ms origins remain unchanged. The publication and
transfer cores retain their own100ms phase bounds; none restarts the enclosing
deadline. The optional `scanout_transfer_check_clocked` entry point observes the
same admitted source clock through the enclosing wrapper, leaving the original
transfer I/O table and legacy check unchanged. A separate stack observer detects
direct changes to the mapping owner's last timestamp during nested callbacks.
Zero, backward, frozen and limit-plus-one clocks refuse; exact endpoints pass.

`make test-intel-command-mapping` exercises actual CPU, transfer, memory,
publication, space and exclusion cores with synthetic hardware. Both100/150ms
policies pass17,808 composed cases, covering every callback's identity/vector/
registration mutation, safe reentry, every clock sample, proof freshness/bits,
uncertain access, retained faults, parent closing and pending readers. Separate
tests cover the nested transfer's own100ms boundary and a reader reappearing
after exact restoration, which must retain backing. Fatal ASan/UBSan,21 compiled
weakening controls, native compiler2KiB frame checks and native-image exclusion
pass. Compile failures do not count as detected weakening controls.

These results qualify composition and failure retention only. Before a native
trial, implement independent firmware/engine/DMA/cache/display-reader ownership
of the exact interval, real ordered completion and the registered original
native adapter/receipt/collector chain. Do not manufacture `HANDOFF_ALL` from
host fixtures or stable PTE observations. Executable context initialization,
engine submission and display/flip ownership remain subsequent work.

The isolated [native admission dry run](intel-command-ownership.md) joins paired
PCI/engine/display/GGTT and CPU alias/cache observations under the original
command hold. It still supplies no transferred interval or GPU completion grant;
its measured requirement6 and missing25 cannot admit this mapping transaction.
