# Broadwell graphics-unit memory drain

The isolated `intel_vtd_drain` core implements one global read/write-drain
operation for the reference Air's graphics remapping unit. It has **no native
adapter, guest-image link or physical acceptance**. It does not change the
inherited-framebuffer desktop, the existing read-only DMA observer, v35 receipts
or any of the 255 open GPU hardware obligations.

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

## Native integration checkpoint

Before any writer image or reboot, integrate all of these together:

- A distinct opt-in image/recipe and versioned raw/strict collection, isolated
  from the old read-only observer and ordinary desktop. The actual writer must
  exist in that image; no synthetic success callback may replace it.
- A child of the **same original** held source/requester/engine lifetime,
  independently registered by that provider. Maintain source/root/epoch,
  requester inhibition and exact original clocks; no standalone recycled
  descriptor outside the owner.
- Exclusive command-register control under the actual kernel/firmware handoff;
  exact DMAR bytes and prior resource policy; one admitted UC writable alias
  with actual page-table/cache checks and explicit retained-failure retirement.
  The accepted read-only alias is not a writable-map grant.
- Live fresh CCMD/FSTS/IOTLB admission and complete original GPU/HDA/CPU/source
  preservation/retirement. No completion-bit promotion beyond this limited
  drain, and no GPU PTE stores.
- Fault/sanitizer adapter tests, exact-stage VM refusal/save/return, recovery
  preservation and an on-disk unattended run record. Show progress in the
  guest, persist exact results and return Ubuntu only after safe retirement;
  otherwise halt visibly. Keep volume1% under existing authorization.

The core and host fixture are the current completed boundary. A VM has no
matching Broadwell remapper and cannot accept the positive hardware operation.
Do not reboot into a known refusal or relabel this host implementation as native
retirement merely to advance a checklist.

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
under `-Wframe-larger-than=2048`. Expanded Make plans for37 default/Intel guest
images omit it. Current strict decoding replays the original raw DMA capture
and confirms both recorded capability values; that is revalidation of old
evidence, not a new native run. Documentation links pass. Durable local evidence
and pinned primary references survive `make distclean`.
