# Broadwell private BCS command completion

`intel_bdw_bcs_flush` implements a command packet and bounded completion protocol
for a **previously owned** blitter ring, context and completion mapping. It is
host-qualified software. No native adapter or guest image calls it, no inherited
ring is submitted, and existing Air7,2 acceptance remains unchanged.

This supplies the command/completion layer described in the
[managed-memory milestones](intel-managed-memory-plan.md#ordered-implementation-milestones).
It cannot create the address-space ownership required to execute its own packet.
The next separately selected [private command-memory trial](intel-command-memory.md)
prepares actual CPU-owned ring/context/completion backing and aliases. GPU
translations and executable context/submission authority remain separate.

## Exact packet

Intel's [Broadwell PRM Volume2a, November2015](https://cdrdv2.intel.com/v1/dl/getcontent/690783),
SHA256 `b035c834a6e1dce2c7057165f2157f8597748bd2b0ccf63a982e0fc4f2dc95b5`,
printed pages834–837, defines BlitterCS `MI_FLUSH_DW`. The pinned Linux v6.12
[non-render flush emitter](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/gen8_engine_cs.c)
and [command definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_gpu_commands.h)
are independent references for command barriers and the generation's fields.

The conservative codec emits six DWORDs:

| DWORD | Value and meaning |
| --- | --- |
| 0 | `0x13044003`: MI opcode26, immediate post-sync QWord, BCS TLB invalidate, five-DWORD command |
| 1 | Direct GGTT completion address with bit2 set; no status-page indexing |
| 2 | Zero high address, restricting this implementation to below4GiB |
| 3–4 | Fresh nonzero64bit completion nonce, low DWORD then high DWORD |
| 5 | `MI_NOOP`, aligning the submission tail to24bytes |

Unlike Linux's status-page-indexed four-DWORD packet, this uses the PRM's direct
GGTT five-DWORD QWord form. There is no notification, interrupt, timestamp,
protected-memory bit, indirect address, arbitrary batch or jump. The completion
address is64byte aligned, avoiding the documented QWord destination-bit5 hardware
restriction. Zero/all-ones nonce and invalid/unaligned/out-of-range destinations
refuse without modifying output. The protocol uses offset0 in a private4KiB
completion page and a separate private4KiB ring. No wraparound is needed.

## Original owner and lifetime

The address-sealed, consumed-once source binds the original owner, provider,
lifetime, context, root, epoch, sequence, nonce, GPU/physical ring and completion
addresses, aperture, protected fence and four enclosing clock origins. Both
pages must be disjoint, aligned and outside the protected fence; physical pages
are conservatively below4GiB. Exact8086:1626 matching narrows the codec; it does
not admit a hardware resource or a sibling Mac.

Before **every** operation, the trusted provider must independently validate
the registered original source and its maintained ownership of:

- a stopped, empty private ring and the exact context that will execute it;
- the exclusive private completion page and fresh sequence/nonce;
- all actual GPU/CPU mappings, physical backing and generations;
- the applicable CPU/GPU cache policy and ordered visibility mechanism.

The proof names that exact source/owner/provider/lifetime/context/root/epoch/
sequence, includes all five prerequisite bits and has a fresh bounded timestamp.
Echoing the descriptor is not hardware evidence. Existing readiness/AF/cache
observations, the accepted remapper drain and System Agent operation supply none
of these ring/context ownership prerequisites by themselves. All prerequisites
must already hold before the first callback; this command cannot bootstrap them.

The provider is a trusted kernel adapter, not a security boundary against arbitrary
memory corruption. The core detects changed identities, callback tables,
registration, packet and progress across every callback, including clocks and
pauses. Reentry, copied sources, overlapping storage, incomplete callbacks and
reuse refuse. No operation returns a hardware token or a handoff assurance.

## Publication, submission and completion

1. Read the actual admitted completion location with acquire ordering. It must
   initially be zero; an already matching nonce is stale and cannot complete it.
2. Publish exactly the six packet DWORDs into the stopped private ring.
   `DONE` means the actual bytes are GPU-visible under the independently admitted
   cache policy. Partial/uncertain publication keeps backing pinned and forbids
   submission. No guessed CPU fence stands in for visibility.
3. Submit exactly that owned context/ring with tail24. Known issuance is separate
   from completion. A missing/uncertain store is terminal; never retry it.
4. Acquire-read the exact completion address until the full nonce matches. Zero
   remains pending; any other value refuses. Immediate completion is allowed
   only after known issuance. The256th poll may complete; no257th poll is allowed.
5. Revalidate the full original ownership and complete packet/progress/count
   record before reporting this request complete. The enclosing owner still
   must retire the command/ring/context/mappings before releasing any backing.

The original preparation policy is100ms by default and150ms only under the
separately selected diagnostic build policy. GT300ms, HDA400ms and outer500ms
origins remain unchanged. An additional **10ms submit-through-completion bound**
starts before the submit callback; a slow submit cannot buy a fresh wait interval.
All bounds include the final ownership validation. Zero/backward/frozen clocks
and exact-limit-plus-one refuse. No existing native deadline is changed.

Every uncertain or failed started lifetime is retained with its first clock
failure and known issuance prefix. There is no automatic retry, cancellation,
rollback, unmap or free callback. Source registration stays consumed after
success or failure. Success is only this BCS command: it does not flush render/
video engines, retire firmware/display readers or certify a completed flip.
The PRM's display-flip caveat on printed page825 still applies.

## Validation and next native boundary

`make test-intel-bdw-bcs-flush` runs the independent actual-C device model under
both100/150ms policies:2,326 cases each. It covers exact encoding, bad descriptors,
all21 binding fields and20 progress/identity fields at all39 callback boundaries,
packet/callback/registration changes, all proof fields, stale/wrong/never-written
completion, write uncertainty, immediate/256th-poll completion, reentry and all
24 clock failures. All five deadlines have exact/+1us cases. Both complete
matrices pass fatal ASan/UBSan;24 compiled weakening controls must fail. Actual
native compiler flags pass the2KiB frame limit under both policies; ordinary,
audit, legacy VT-d and combined GGTT image plans exclude this caller.

Next establish a genuine private ring/context/status/completion mapping owner,
including initialization and required visibility **before submission**, and
its original-source retirement contract. Then integrate ordered native callbacks,
raw receipts and complete failure/return gates in a separate image. A fixture
or a saved earlier receipt cannot substitute for that live prerequisite.
No physical trial is scheduled merely to demonstrate the known missing owner.
The [tear-free roadmap](intel-tearfree-roadmap.md) still requires every remaining
DMA/prefetch, GPU pipeline/cache/translation and firmware/display-reader witness
before a managed mapping/publication grant. All255 obligations remain open.
