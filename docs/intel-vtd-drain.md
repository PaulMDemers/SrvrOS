# Broadwell graphics-unit memory drain

The isolated `intel_vtd_drain` core implements one global read/write-drain
operation for the reference Air's graphics remapping unit. The distinct opt-in
**v36 native adapter** joins it to the complete original v35 held lifetime.
Native qualification/physical acceptance is pending. The inherited-framebuffer
desktop, old read-only DMA observer and v35 receipts remain independent, with
all255 GPU hardware obligations still open.

## Why this mechanism

The accepted read-only observation, runtime `d5ad0c54`, release
`53460fd7c2c0f337`, session `session-00065f6cb059ee67-00`, already supplies:

| Field | Captured value |
| --- | --- |
| GPU | `8086:1626`, segment0/bus0/device2/function0 |
| Exact DMAR hash | `0x722775ad66626863` |
| Graphics DRHD | `0xfed90000` |
| VER / CAP / ECAP | `0x10` / `0x01c0000c40660462` / `0x7e1ff0505e` |
| GSTS / RTADDR | `0` / `0` in both snapshots |

CAP reports both read and write drain support. ECAP.IRO is `0x50`, locating
the invalidate-address register at `0x500` and IOTLB register at `0x508`.
Reuse this evidence; another unchanged observation boot is unnecessary.
See the [resource admission and original result](macbookair7-2-dma-observation.md#native-observation-accepted-2026-11-04).

Intel's [VT-d architecture specification, revision4.1](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf),
sections6.5.1 and6.5.4, specifies register invalidation independently of TES/IES,
with queued invalidation disabled. A global invalidation with DR and DW completes
the supported memory drains before IVT clears. Writes reach the point where CPU
accesses can observe them; reads finish fetching their memory response data.
The specification's explicit disabled-state requirement avoids enabling
translation just to obtain a drain.

The Broadwell datasheet, **330835-003, June2015, volume2**, sections4.3.2,
4.3.3,4.3.7 and4.3.31, gives the matching capability fields, context-command
interlock and IOTLB layout. The retained copies have SHA-256:

- VT-d4.1: `b0c122a5fa927578ff963636dd3abe87aed0a28f73bbf50e74dc54c099bf4aeb`.
- Broadwell330835-003: `22368ca371590a6eb5d85c02256936d6eb92badc27d2345e5d6ce8bee9c128b3`.

Do not substitute graphics remapping activation. Pinned
[Linuxv6.12 graphics quirks](https://github.com/torvalds/linux/blob/v6.12/drivers/iommu/intel/iommu.c)
disable it for Broadwell, including `0x1626`. This unit's Ubuntu graphics device
also has no IOMMU group. That is consistent with the quirk, not proof that every
disabled-unit command works on this machine. A native drain still needs its own
bounded trial; no remapping-enable, root-pointer or queue writes are proposed.

## Operation and refusal

The one-use source binds its original owner, provider, lifetime, source epoch,
operation sequence, CPU root, exact topology/unit, GPU ID and original
preparation/GT/HDA/outer starts. The provider must independently match that
descriptor's address and registered child against the actual ownership graph.
An echoed description or a clear BME/TES bit cannot supply this registration or
exclusive invalidation-register control. Only scalar binding/progress/callback
data is copied for callback-integrity checks; ownership objects are not copied.

1. Admit the original lifetime and all **100/300/400/500ms** budgets, without
   starting a new allowance. Fresh ordered UC DWORDs bracket VER, stable
   high/low/high CAP, ECAP, RTADDR and CCMD with GSTS. Require the exact captured
   capabilities, RTADDR0, GSTS0 and FSTS0. CCMD must be its conservative default
   `0x0800000000000000`, including no ICC request. This last field was not in
   the earlier capture; the future writer must check it live rather than claim
   the old observation accepted it.
2. Read IOTLB high/low/high before any write. Only the reset value
   `0x0200000000000000` or a prior exact completed global DR/DW operation
   `0x1203000000000000` is accepted. Refuse pending, torn, floating, reserved or
   differently configured state. Recheck GSTS, CCMD and FSTS before issuance.
3. Write **one DWORD**, offset`0x50c`, value`0x90030000`: IVT, global IIRG,
   DR and DW. The upper DWORD includes the register's triggering upper byte.
   Never write its reserved low DWORD, invalidate address, root table, GCMD,
   context command, queue, fault status or the other remapping unit.
4. Poll at most256 fresh upper DWORDs, with original-lifetime guards, clocks,
   GSTS/FSTS and stable CCMD throughout. Require global+DR+DW request fields;
   only IVT/IAIG may change. After a definitely issued request, immediate
   self-clear is legal. Clear IVT must report **IAIG=global**, not ignored,
   domain or page granularity. A missing busy sample is not a failure by itself.
5. Repeat the full context, verify the reserved low DWORD and exact final high
   DWORD, and perform the final original guard/clock check. Validate counters,
   response trace, known issuance and advancing time before completion.

Zero/backward/expired/frozen clocks, failed callbacks, changed binding/epoch,
rolled-back progress, incorrect granularity or uncertain writes are terminal.
Every admitted failure retains its original enclosing lifetime. There is no
retry, command clearing, forced release or rollback API. Even success consumes
the descriptor; it is a completion witness within the held owner, not a release
or mapping capability. Raw context, initial/final command, poll responses and
first failed clock remain available for a future strict receiver.

## What completion would establish

Only **AT=00 requests to memory already processed by this graphics remapper**
are in scope. It does not cover peer destinations, ATS translated requests,
later requests or work still inside GPU pipelines/caches/prefetch queues.
Request generation must remain controlled if a later phase needs the drained
state to persist. A unit-wide drain is not a per-page allocation or source
ownership proof, and the other include-all DRHD stays untouched.

In particular, this cannot supply the independent flushed-pipeline prerequisite
of the [five-engine TLB protocol](intel-bdw-tlb-protocol.md). Do not turn a
successful drain, reset ACK or ready-for-reset state into that prerequisite.
Cache maintenance, every firmware/display reader and the source-bound private
GPU range/publication/restoration still need their own evidence.

## Native v36 adapter and collection

`build/intel-vtd-drain/srvros.elf` and `--intel-vtd-drain` select the distinct
15-token `srvros.intel-ring=vtd-drain-v1` recipe. Other images omit the writer.
Only this image defers user services with `vtd-drain-preinit` and emits wake
version36. The guest explains that no input is needed, saved safe outcomes
return automatically and unsafe retained state halts visibly.

The adapter retains the entire original private CPU preparation and v35
PCI/cache/alias/inventory/reader audit. After that audit, still inside the
original inhibited requester and five-engine hold, it registers its original
source/result as a dependent of that same provider. It uses the existing
cooperative EFI boot-observer/exclusion lease, IRQ exclusion and original
source/root/epoch/GT/HDA/outer clocks. This is bounded OS command serialization,
not proof of SMM retirement or a generalized IOMMU/firmware ownership grant.

Before mapping, the port checks exact DMAR bytes and the original resource
policy, fresh PAT/MTRR context and an empty dedicated VA. It maps one NX,
supervisor, writable4KiB UC alias at `0xffffc001f5150000`. Every register callback
checks the original ownership graph, live registration, actual translation and
entry attributes, fresh equal CPU cache context and original deadline. A clean
operation removes that exact alias, verifies absence and checks policy/clock
again before dropping its registration. Any uncertainty retains the enclosing
source/requester/engine/HDA/backing lifetime. Nothing clears a pending request.

The requester restoration gate independently rechecks the complete original
drain record: source and both bindings, callbacks, counts, full control/poll
trace, all four clock limits, actual alias retirement and enclosing child order.
A later damaged record cannot authorize restoring BME, even if the nested
operation had once completed. The old complete audit must also pass its own
independent return gate.

Historical `intel-vtd-drain: v=1 b=...` carries a canonical1552-byte little-endian record:
50uint64 header words, two eight-word contexts and256 DWORD poll positions.
The header includes original clocks/source, first failed clock, all counts,
initial/final controls, map/retire/registration, actual alias/cache attributes
and core flags (complete/busy/retained/original object/source/consumer).
`intel-vtd-table: v=1 b=...` preserves the independently decoded136-byte DMAR.
The receiver checks the operation against the original audit and its independently
parsed GT/HDA/outer binding, and independently computes the native UC policy.
No copied v35 receipt, foreign recipe/version, missing/duplicate record or false
completion can pass. Raw session files are saved before normalization/decoding;
the report retains the actual wake version36.

The [manifest workflow](device-workflow.md) selects `intel-vtd-drain-v36`, keeps
the old full audit matrix and adds port/core/boot/image qualification. The local
captured DMAR fixture is mandatory and content-bound; raw firmware stays out of
Git. Exact-stage QEMU validates refusal, saving, unmount and return only. Before
physical execution, preserve recovery and matching durable receipts, use the
local USB identity and volume1%, and collect the matching result on Ubuntu.
A VM has no matching Broadwell remapper and cannot accept the positive operation.

## Host qualification

`make test-intel-vtd-drain` passes20,154 actual-core cases, fatal ASan/UBSan
and23 compiled weakening controls. The matrix covers every callback position
of a multi-poll lifetime with source/progress/I/O drift, every clock/guard/read
failure including polling pauses, immediate completion,256-poll completion and
pending refusal, both admitted initial states, reentry/reuse, uncertain stores,
torn/foreign/reserved/floating controls and wrong completion granularity.
The original four deadline boundaries pass at their exact limit and refuse+1us;
a wholly frozen clock refuses final completion.

The core compiles with both GCC freestanding and the actual Zig kernel flags
under `-Wframe-larger-than=2048`. The original host-only checkpoint checked37 default/Intel image plans. The
new explicit v36 image alone links the core; ordinary and v35 images omit it. Current strict decoding replays the original raw DMA capture
and confirms both recorded capability values; that is revalidation of old
evidence, not a new native run. Documentation links pass. Durable local evidence
and pinned primary references survive `make distclean`.

The port fixtures cover both original entry routes, target faults, permission/
cache failures, every original callback boundary, no later register access after
a broken boundary, and the final original-record return gate. Fatal sanitizers,
compiled weakening controls and raw-first collector tests accompany them.
The private local `build/device-fixtures/macbookair7-2-dmar.bin` is checked against
the exact captured table; hosts without it skip these unit-specific positive
fixtures. Generic protocol and exact boot-gate tests remain available without
firmware. Frozen-source full qualification and native acceptance are tracked
in the local workflow run, not inferred from an initial model result.


### Native deadline failure and bounded retry

The first v36 native run (`b84dc1ef`, release `59f18ff1270fa5bd`) records one
expected global DR/DW completion, then fails the last verification at the
original preparation budget: last successful core clock99,915us, parent
rejection100,001/100,000us.49/50 register reads complete. The complete lifetime
is rejected; the alias and enclosing ownership remain retained. Offline files,
allocation and recovery verify. This does not accept even a completed nested
drain without its original full return.

The retry validates pure original identity before the clock callback, then
performs the full hardware census after that callback. It removes the duplicate
pre-clock census, keeping every predicate and the fresh final alias barrier
before register access. No deadline or polling bound changes. Phase envelopes
and the actual rejected timestamp use existing clock samples, with **no added
clock reads**; they describe inter-sample intervals, not exclusive function costs.
The canonical version2 receipt appends16 uint64 words (1680bytes total): calls,
full parent checks, failed timestamp, previous timestamp, failure stage (1parent
admission,2parent deadline), first/last sample, largest gap/ending phase, and
seven ending-phase interval totals. Failed zero/backward clocks preserve their
last valid prefix. Historical version1 remains decodable; new frozen manifests
require version2, so removing timing data cannot satisfy the new trial.

The collector allows a completed local audit followed by a retained failed
drain only inside that same faulted, registered, blocked, unreturned child.
The older v35 rule stays strict. CPU preparation is validated locally without
promoting full-chain completion. A well-formed negative run can be collected
for rejection; incomplete chains cannot be accepted through a supplied proof.


### Later return evidence and clock-only admission

Retry `8e26448a` / release `442502e3c2fc8288` records two matching original
kind2 runs. Both complete the drain (9,697/9,723us), retire its UC alias and
restore engines and requester by99,558/99,724us from preparation. The next
required full hardware validation takes1,596/1,592us; the enclosing transfer
then rejects101,154/101,316us. Both complete chains remain **incomplete**.
All prior files, offline allocation and installed recovery verify. Both raw
sessions are retained; unexpected multiplicity does not select a passing run.

The next adapter keeps clock callbacks to original software/CPU-root/alias/HDA
borrow identity and original time propagation. They authorize no hardware access.
All guard callbacks retain full original hardware admission. Register loads and
the single store still have a full hardware check after the final clock/alias
checks; CPU alias map/unmap and final child deregistration gain the same fresh
hardware boundary. The closing full census and all100/300/400/500ms limits stay
unchanged. A changed HDA endpoint, HDA alias or HDA borrow at a clock boundary
must prevent later register I/O, alias mutation and deregistration. Compiled
controls remove those individual closing checks to verify the barriers.

Declared-gap resource admission already validates every exact DMAR byte, so
that admission is used once per policy check. Reserved-region admission still
performs its explicit full topology validation; a reserved-region bad-table
case must stop before any map/write. Ordinary/read-only resource policy is
unchanged. Both model routes retain107 fresh cache and157 alias checks, while
full parent attempts fall from369 to161 in the immediate-completion model.
This is a call-count result, **not** a native speedup or ownership acceptance.
Version2 timing and all original protocol/return predicates remain mandatory.
