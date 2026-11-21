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

The default100ms and selected150ms legacy preparation policies retain their
GT300ms, HDA400ms and outer500ms origins. Named aggregate aliases also select
the existing separate practical250/500/750/1000ms preparation/GT/HDA/outer
policy; no legacy value changes and no origin restarts. An additional **10ms submit-through-completion bound**
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
the100/150ms legacy and250ms practical policies:2,326 cases each. It covers exact encoding, bad descriptors,
all21 binding fields and20 progress/identity fields at all39 callback boundaries,
packet/callback/registration changes, all proof fields, stale/wrong/never-written
completion, write uncertainty, immediate/256th-poll completion, reentry and all
24 clock failures. All five deadlines have exact/+1us cases. Both complete
matrices pass fatal ASan/UBSan;24 compiled weakening controls must fail. Actual
native compiler flags pass the2KiB frame limit under all three policies; ordinary,
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


The [private BCS execlist submit protocol](intel-bdw-bcs-submit.md) now supplies
the host-qualified four-DWORD port operation beneath the future submit callback.
Actual joined-core tests keep nonce completion separate from port issuance and
retain backing on every partial/uncertain/late outcome. This still requires
already owned initialized context/mappings; it cannot bootstrap their transfer
or certify other engines, firmware or display readers. No native adapter exists.

## Physical Ubuntu reference probe

`tools/intel_bdw_bcs_host.c` uses the integrated GPU's Linux i915 render node.
It requires Broadwell `8086:1626`, blitter support, a shared LLC and full
per-process graphics translation. Each case creates a private context, batch
and result page. i915 resolves both address relocations within that context.
The positive case issues `MI_STORE_DATA_IMM` and then `MI_FLUSH_DW` with
immediate QWord post-sync data on a separate cache line. Both halves of the
marker are fresh and nonzero. The probe reads the prefaulted write-back
mapping and checks the earlier value when it observes the marker, before it
calls the userspace fence wait. After the bounded wait it checks the whole
result region, including zero padding. The control ends after the first
write and leaves the marker at zero.
[Intel Broadwell PRM Volume 2a, pp. 834–837 and 915–917](https://cdrdv2-public.intel.com/v1/dl/getcontent/690783).

```sh
mkdir -p build/tests
cc -std=c11 -O2 -Wall -Wextra -Werror tools/intel_bdw_bcs_host.c -o build/tests/intel-bdw-bcs-host
timeout 15s build/tests/intel-bdw-bcs-host
```

On 2026-11-18, MacBookAir7,2 completed 20 positive cases and 10 controls.
The host observed the marker after 0–135 µs of polling after the submit
ioctl returned. A fatal ASan/UBSan build repeated one set successfully.
The wait and other scheduling work in i915 can affect these times. The
probe exercises private PPGTT addressing without requesting TLB invalidation.
Native GGTT addressing, ring and status memory admission, firmware exclusion,
and completion across other engines and display readers remain separate.

## Five-engine private queue test

The Linux comparator uses the SrvrOS Gen8 packet encoder with private
32-bit GPU addresses. i915 pins the result and batch pages at the specified
addresses. The command stream needs no relocation. RCS uses `PIPE_CONTROL`
with CS stall, render target, depth and DC flush plus a post-sync QWord.
BCS, VCS0, VCS1 and VECS use `MI_FLUSH_DW` with a post-sync QWord.
The native GGTT blitter trial uses a separate packet with TLB invalidation.
[Linux Gen8 command emission](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/gen8_engine_cs.c)

Build on the Air's Ubuntu installation. The privileged shell opens a fresh
software timeline. The render user runs the probe and opens the DRM devices.

```sh
mkdir -p build/tests
cc -std=c11 -O2 -Wall -Wextra -Werror -Ikernel/include \
  tools/intel_bdw_engine_drain_host.c kernel/diagnostics/intel_bdw_bcs_flush.c \
  -o build/tests/intel-bdw-engine-drain-host
sudo sh -c 'exec 3<>/sys/kernel/debug/sync/sw_sync; exec setpriv --reuid="$SUDO_UID" --regid="$SUDO_GID" --init-groups timeout 20 "$1" 3' sh "$PWD/build/tests/intel-bdw-engine-drain-host"
```

Each engine writes data and then its marker on a second cache line. The
CPU checks those private mappings before waiting on the completion fence
and verifies the results with an independent GEM read. Each successor
waits on the same software fence. The internal panel must advance on
its original CRTC while all five successors remain pending with both
locations untouched. After release the CPU verifies all successor data
and markers. This exercises private work scheduled by i915. Native
global admission, a power-manager L3 flush and slot ownership still
require independent evidence.
