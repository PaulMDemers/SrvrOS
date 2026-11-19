# Broadwell translation request/completion protocol

`intel_bdw_tlb` implements the bounded software sequence for five per-engine
TLB invalidation requests. It has **no native adapter or guest-image caller**.
The inherited-framebuffer desktop and all existing hardware acceptance remain
unchanged. This component closes a protocol implementation task, not a GPU
ownership obligation.

## Hardware requirement

Intel's [Broadwell PRM Volume 2c, November 2015](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA-256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages 180, 973–974, 1327 and 1713 define these DWORD controls:

| Engine | Control offset |
| --- | --- |
| RCS | `0x4260` |
| VCS0 | `0x4264` |
| VECS | `0x4270` |
| VCS1 | `0x4268` |
| BCS | `0x426c` |

Writing bit 0 requests invalidation; hardware clears it upon completion. Bits
31:1 are reserved. The manual requires the corresponding hardware pipeline to
have been flushed and cleared of its memory accesses first. An initial zero
therefore establishes neither an issued request nor a completed operation.
A request can complete before the first ordered read; observing a set bit is
not mandatory after a definitely issued write.

The pinned manual is retained locally under the earlier nonzero-reset reference
checkpoint. Its hash is checked independently when qualifying this protocol.
The current [completion diagnostic](intel-completion-prerequisites.md) only
observes those controls and continues to perform no TLB writes.

## Original lifetime and operation

The one-use source descriptor registers its original result. It binds the
provider/context, owner and lifetime, source epoch, operation sequence, CPU root,
private physical span, exact GPU range, protected fence and original preparation,
GT, HDA and outer start times. The core admits only the conservative Broadwell
`8086:1626` profile, below-4-GiB private backing and at most 1,536 pages outside
the fence. A future native adapter must still match the actual machine and all
resource/cache/allocator policy; the GPU ID alone is not that admission.

The result captures only binding/progress data. It does not copy an ownership
object or synthesize an exclusion token. Every callback must preserve source,
registration, original callbacks and protocol progress. Copied/rebound sources,
reentry, changed scope or rolled-back counters fail before a later request.
The provider must independently match the descriptor against its original
registered child and actual enclosing ownership graph. Echoing the supplied
address, masks or fields is not a valid proof. This is a trusted kernel-adapter
contract, not a boundary against a malicious provider rewriting owned storage.

The sequence is:

1. Require a fresh, independently established prerequisite proof for all five
   flushed pipelines and their retired memory accesses. Bind its source, owner,
   provider, lifetime, epoch, sequence, root and checking time to the original.
2. Read all five initial controls before issuing any request. Reject a pending
   bit, floating value or reserved bit without trying to clear or overwrite it.
3. In engine order, revalidate the prerequisite, issue exactly one ordered UC
   DWORD value `1`, and classify the write as definitely issued, definitely not
   issued, or uncertain. A definitely issued write still needs completion.
4. Require ordered reads of that exact control until it clears, capped at 256
   reads per engine. Revalidate the prerequisite and original clocks throughout.
   A pending or failed engine prevents requests to later engines.
5. Perform the final prerequisite check and validate all raw/control/count
   prefixes. Success needs exactly five known requests, five self-clears, the
   full requested/completed masks, consistent reads/proofs and advancing time.

The core always checks the original **100/300/400/500 ms** preparation/GT/HDA/outer
budgets. It does not start another allowance when a request begins. Zero,
backward, expired or wholly frozen clocks fail. Deadline samples and their first
cause remain in the failed result.

A possible request, binding loss or uncertain completion retains the enclosing
lifetime. There is no forced rollback, request clearing, resource release or
retry API. Even a no-request refusal is not permission to free the caller's
backing; its original owner must perform its own clean retirement. The descriptor
and result remain consumed after success or failure. A later mapping/restoration
phase needs a new provider-admitted sequence and descriptor, never an old result.

## Native provider gap and next decision

Current native readiness ACKs, AF TP clear, CPU writeback, stable cache mappings
and sampled idle controls do **not** provide the required pipeline/memory proof.
No fallback converts those observations into the new prerequisite masks. The
new core consequently cannot be attached to the current v35 callback and made
to issue requests legally. No new recipe, receiver version or physical trial is
introduced merely to demonstrate that known refusal.

Before adding native I/O, select and document a complete quiescence mechanism:
an owned command/context completion path or a controlled reset/DMA-retirement
path with the necessary scope. Establish its actual memory-retirement guarantee,
maintained firmware and reader exclusion, source/epoch, exact resource policy
and restoration outcome. A command path must not depend on a GGTT ownership
grant that the same command is supposed to establish. Reset readiness or reset
acknowledgement alone cannot fill in the missing guarantee.

The [disabled-state VT-d drain protocol](intel-vtd-drain.md) now implements one
documented root-complex memory-drain mechanism against this unit's captured
capabilities and now has one source-specific bounded native kind2 lifetime.
It remains deliberately narrower than this
TLB prerequisite: AT=00 memory requests already processed by the remapper do
not cover remaining GPU pipeline/cache/prefetch work or display readers. It
must not be wired in as a positive flushed-pipeline/memory-retirement mask.

The separate [System Agent GGTT invalidation protocol](intel-ggtt-invalidation.md)
now accepts one bounded original kind2 native101008 write/posting-read lifetime.
That different translation domain does not satisfy the engine pipeline prerequisite.
The [private BCS command/completion layer](intel-bdw-bcs-flush.md) is host-qualified,
but has no native provider and requires already-owned ring/context/completion
mappings. Neither an earlier receipt nor a positive fixture supplies this
protocol's live prerequisite.

The [physical Ubuntu reference probe](intel-bdw-bcs-flush.md#physical-ubuntu-reference-probe)
observed a fresh blitter marker and the earlier private write before calling
the i915 userspace fence wait. It exercised an i915-owned PPGTT and did not
request invalidation. A native provider still needs separately admitted
command mappings and retirement evidence.

Then embed the operation inside that **same original** live owner, qualify
native alias permissions and ordered accesses, and compose request completion
with PTE publication and restoration. The existing transfer/publication cores
still require their independent posted-write, cache, DMA and reader proofs.
This operation's success would cover only its issued per-engine invalidations;
it does not issue a `HANDOFF_*` assurance, clear mask 255, map a page or retire
a display reader. There is no reason to reboot until that new native operation
has a real prerequisite and a bounded hardware question.

## Validation

```sh
make test-intel-bdw-tlb
```

The actual C core is tested against a synthetic device, with 6,143 cases covering
all 20 source fields and 16 progress fields at every callback boundary, all
proof fields, every clock/read/write fault, reserved/floating/pending controls,
uncertain writes, bounded polls, copied/rebound objects, mandatory callbacks and
exact original deadlines. Faults assert that no later request occurs.

Fatal ASan/UBSan runs the same matrix. Nineteen compiled weakening controls check
register order, prerequisites, source/registration/generation/time binding,
reserved bits, write uncertainty, completion and count gates, and one-microsecond
deadline extensions. Two controls remove both callback-progress and terminal
checks of the same invariant; the model must still catch the missing invariant.
An independent image-isolation check forbids the protocol in guest callers.
All of these are host software evidence, not physical completion acceptance.
