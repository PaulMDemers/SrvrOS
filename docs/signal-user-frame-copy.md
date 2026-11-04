# Bounded signal-frame publication: next design packet

Status: proposed after the qualified common proof and owner-reviewed private
restore candidate. No copy helper, caller, action syscall or signal delivery is
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
