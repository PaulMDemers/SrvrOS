# Bounded signal-frame publication: next design packet

Status: revised design following qualified common proof and private restore
candidates. The owner requested the exact restrictions below before code. No copy helper, caller, action syscall or signal delivery is
implemented by this packet. The process/VMM owner must approve the exact admitted
mapping and CPU policy before implementation.

## Present source and scope

`map_process_page_mode` allocates a fresh PMM_FRAME_USER page, zeroes it, installs
a4KiB mapping and records virtual/physical/flags. Fork copies pages eagerly.
There is no COW/demand resolver for those tracked mappings. Existing generic
`copy_user_bytes` permits a prefix and releases exclusion between pages. It must
remain unchanged for its current callers; it is not sufficient for publishing a
complete signal frame.

The new operation is internal and current-process-only: a trusted resident kernel
source, a nonzero length at most1024, a lower-half destination and an already
validated process/image/root identity. It establishes the whole destination and
source before the first destination store, then performs exactly the bounded
scalar copy under the same uninterrupted CPU0 IRQ exclusion. It does not return
a reusable plan, pin mappings for later, allocate or resolve a missing page.
All admitted destination mappings are ordinary tracked4KiB process-owned RAM;
large leaves, device/shared/untracked mappings and inconsistent ownership refuse.

## Proposed validation and transaction

1. Require IF0, actual registered nonkernel CR3 equal to the expected pure root,
   and the exact current process/worker/image binding. Signal delivery/return
   callers additionally require the current common FPU proof and strict safe
   return observation; a bare process pointer or old cached binding is insufficient.
2. Require nonzero lower-half destination and length1–1024, complete checked
   arithmetic and canonicality. At most two destination4KiB pages are involved.
   Frame planning separately checks red-zone preservation, alignment, stack
   overflow and nonoverlap with all active public frames before this operation.
3. Preflight every covered page without callbacks. Require4KiB leaves, PRESENT
   and effective USER/WRITE across all levels; exactly one matching tracked
   process mapping for each virtual page; exact actual PTE physical address equal
   to its nonzero aligned recorded owned RAM frame. Bounds-check mapping count
   before scanning and retain no duplicate/partial match. No COW/device/advisory
   writable flag substitutes for the actual resident writable leaf.
4. Check actual relevant paging/access policy without changing it. Proposed
   narrow profile is4-level paging, CR0.WP=1 and CR4.SMAP=0; protection-key or
   other active access policy that could fault the excluded copy must also be
   explicitly accounted for or refused. NX-bearing entries require real NX/NXE
   support. The known Q35 profile has WP1/SMAP0/NXE1; it does not establish other
   machines' policy. No STAC/CLAC, CR0/CR4/EFER write, trial access or new fault
   recovery is proposed. Exact additional policy bits remain an owner decision.
5. Source is an aligned fixed private frame object owned by the caller, stable
   and resident for the call, never a user/wire pointer. Check complete numeric
   extent, canonical upper addresses and its kernel mappings. Translate each
   covered source extent and reject physical overlap with any destination extent,
   including direct-map aliases; virtual nonoverlap alone is insufficient.
   Source/private records, current common reservation/frame, selected FPU objects
   and scheduler stack bookkeeping must remain private. Process ownership of the
   destination must exclude all of those kernel objects. Numeric checks alone do
   not authenticate allocation or provenance.
6. Only after complete validation, copy exactly the requested bytes using a
   scalar bounded loop without allocation, callback, IRQ reenable, scheduling,
   service lock, I/O or mapping mutation. No failure return remains after the
   first store. Unexpected hardware/mapping faults are terminal invariant failure,
   never recoverable partial signal publication. Refusal leaves the entire user
   destination and all pending/action/mask/private/live-frame state unchanged.

The process mapping record is kernel ownership bookkeeping, not a new per-frame
PMM capability API. This relies on the existing fresh unique PMM allocation and
on admitted user/private kernel frames not being DMA targets. IF0 cannot stop
devices. The owner must confirm the admitted source/destination allocation paths
satisfy that ownership restriction; do not infer it merely from aggregate tag
counts. Do not create a generalized allocator/pinning subsystem for this slice.

Existing active4KiB map/protect/unmap paths issue INVLPG. This proposal changes
no TLB policy and does not repair or certify arbitrary preexisting stale
translations. CPU0 exclusion must span preflight and copy with no service window;
construction/cleanup functions with checkpoints cannot run inside the transaction.

## Future publication ordering and qualification

Before copying a signal frame, prepare every private/public byte and validate
all pending/action/mask/depth/token/entry and FPU preconditions. A successful copy
must be followed only by prevalidated nonfailing local commit of private record,
pending bit, mask, consumed witness and live trampoline frame. The current
separate consume helper can refuse; the later delivery transaction must arrange
and review its exact prevalidation/commit order before any user store. This
packet does not claim that integration exists. Action replacement, syscall
admission, sigreturn RAX bypass, nesting and restart epochs remain later contracts.

Actual-source tests must use page guards and two-page destinations, fail the
last preflight before any store, independently remove PRESENT/USER/WRITE at each
level, refuse large/untracked/duplicate/mismatched physical mappings and wrong
root/image/IF/policy, and cover exact/partial physical aliases through different
virtual addresses. Assert unchanged bytes/state on every refusal, exact bounded
successful copy, no allocator/free/I/O/IRQ/scheduler/console callbacks, and no
write beyond either end. Inspect target scalar codegen and exercise the admitted
path on a matched guest before enabling signal/public ABI callers.


## Exact narrow policy revision for owner review

The initial source is a fixed aligned1024-byte local object on the current owned
kernel stack. Main ownership is the process resolver's validated PMM_FRAME_KERNEL_STACK
allocation (base/physical/32frames); worker ownership is the current scheduler
record's validated32-frame stack allocation. Both are128KiB direct-map extents.
Require the exact observed selected stack top and current RSP within that extent,
then complete source containment below the common reservation/frame without
wrapping. Extend the fresh internal observation with the owned worker base and
physical extent, and the process binding resolution with main base/physical;
do not infer ownership by subtracting128KiB from an arbitrary top pointer.
The boot monitor/default GDT stack and arbitrary heap/private-record sources
are not admitted. A future signal caller constructs its public bytes in this
local object from validated private records before invoking the operation.

Require actual CR0.PE, PG and WP, CR4.PAE and EFER.LMA. Refuse CR4.LA57, SMAP,
PKE, PKS, CET and PCIDE. Require raw CR3 to contain only a nonzero page-aligned
physical root equal to the validated expected root, with no PCID/PWT/PCD/no-flush
or other policy bits. This does not enable or repair any policy. CPUID must
provide MAXPHYADDR through extended leaf0x80000008, with a supported36–52-bit
width. Reject encoded physical bits above that width before following a table
or accepting a leaf. Use actual NX capability/EFER.NXE for any used NX bit.
Require CPUID PAT support before reading IA32_PAT, and require slot0 to encode
write-back RAM. No PAT or MTRR settings are changed. We establish identical
admitted PAT indices for aliases, not universal effective MTRR memory types.

For nonleaf entries admit only PRESENT, WRITABLE, USER, ACCESSED, an aligned
MAXPHYADDR-bounded physical table address and supported/enabled NX. Reject all
other bits, including software-available, cache, key and reserved encodings,
before following the pointer; reject PS in PML4 and any large destination leaf.
Destination4KiB leaves admit PRESENT/WRITABLE/USER, ACCESSED/DIRTY, supported NX
and a bounded aligned physical address. Refuse leaf PAT/PWT/PCD, GLOBAL, software,
key or reserved bits. Require effective USER/WRITE and exactly one matching
tracked ordinary process page; metadata flags must also describe the admitted
ordinary USER/WRITE/NX mapping with no unknown/cache/device flags.

For current kernel-stack source translations, permit existing4KiB,2MiB and1GiB
HHDM leaves with effective supervisor/RW permissions. Admit GLOBAL only on these
source leaves; otherwise use the same understood bit set, with PS only on the
appropriate PD/PDPT leaf and PAT/PWT/PCD still zero. Require all source-level
USER bits clear. A1GiB leaf requires real CPUID Page1GB. Large-leaf address
alignment/reserved-low-bit rules must be checked separately from the PAT bit;
never interpret PS/PAT as physical address bits. Match every translated source
byte extent to `owned_physical_base + (address-owned_virtual_base)`. This strict
proposal may refuse a bootloader HHDM with different encodings; measure it in
the admitted-path guest and return to review if needed, rather than silently
widen flags. Tables are private resident allocations or inherited boot paging
structures of the validated registered root; this does not authenticate arbitrary
attacker-supplied table memory or repair corrupted kernel allocation bookkeeping.

Compare every destination physical byte extent against the source extents,
the entire current contiguous kernel-stack allocation and translated complete
selected user/kernel FPU objects. Refuse exact/partial aliases before writing.
All protected objects are obtained from the fresh validated owners, never wire
pointers. Future signal private-record/action storage additionally needs its
own reviewed ownership/nonalias contract before production publication wiring.

Admitted user-page producers are map_process_page_mode via ELF segments, initial
stack, TLS blocks, brk/heap growth, mmap, and eager fork copying. All allocate
fresh PMM_FRAME_USER pages; moves transfer ownership, they do not create sharing.
Retirement/protection uses tracked unmap/cleanup, munmap/mprotect and process/TLS
cleanup. Current source-stack producers are allocate_kernel_stack and scheduler
spawn stack allocation, both PMM_FRAME_KERNEL_STACK; process/scheduler teardown
must invalidate bindings before freeing them. Preserve the qualified resolver/
association invalidation order and exclude every allocator/checkpoint/retirement
path from this IF0 transaction. Ownership depends on the existing unique PMM
allocation contract, not on counting tags as an authentication step.

Inspected AHCI, xHCI, e1000, audio and virtio block/net/input/rng/GPU paths allocate
separate PMM_FRAME_DMA rings/bounce/data storage or use controller MMIO. AHCI read/
write performs CPU copies to/from its private io_buffer. No direct user-frame DMA
submission was found in these admitted producer paths. The required invariant is
that admitted USER and current KERNEL_STACK allocations remain uniquely owned and
are not submitted to devices; IF0 alone cannot establish it. Any future direct
user DMA/shared/device mapping is outside this admission contract and must not be
classified as an ordinary tracked user page without revisiting this gate.

A genuine guest test will use an explicitly opt-in test kernel with a private
INT3 self-test hook, separately reviewed before code. A real main/worker common
CPL3 frame and current proof supply a validated stack-local source and a tracked
one/two-page user destination. The hook invokes the actual helper before normal
return, records copied bytes/refusals and actual access/HHDM/PAT policy, and does
not allocate a public syscall or advertise a feature. Receipt binds the separate
test-kernel hook and helper object; normal production kernel remains unwired and
runs the unchanged regressions. Guest coverage must not be described as a live
production caller. The exact opt-in hook and final publication/consume integration
remain separate approval decisions; this revision requests policy/provenance
agreement before implementation.


Paging encodings and access controls follow
[Intel SDM volume3A sections4.5/4.6 and reserved-bit tables](https://cdrdv2-public.intel.com/819714/253668-sdm-vol-3a.pdf).
The strict allowed-bit policy above intentionally refuses some architecturally
ignored encodings. It does not label those encodings reserved on every CPU.


## Mapping scan and timing qualification

The current two-page process preflight performs at most four complete scans of
PROCESS_MAX_MAPPINGS=65536, or262144 record visits under IF0, in addition to fixed
identity/policy/page-walk work and at most1024 user-byte stores. The byte limit is
not a total latency bound. Maximum-table host cases place valid targets near the
end, then inject duplicate VA and physical ownership at the last record and
require unchanged destination on refusal. Target measurement of worst admitted
scan+copy latency on this emulated profile is required before a production caller.
If too long, return for reviewed bounded lookup/preparation; do not add service
windows to the atomic transaction or silently cap away large browser images.
Actual guest accesses may update architectural A/D bits; host table-byte identity
checks establish absence of software mutation in the model, not frozen hardware
paging flags. The optional test-kernel hook is approved only with bounded serial
failure output/stable scalar record, read-only helper timing and separate object/
kernel receipts; it does not approve signal publication or a production caller.
