# Native command ownership prerequisites

The isolated `intel-command-ownership-v38` trial adds a native admission dry run
between the original private CPU command child and its parent return. It joins
fresh hardware observations under one requester/engine/RAM hold. It does not
supply a transferred interval or complete GPU DMA/cache/reader ownership; those
remain prerequisites of the [mapping transaction](intel-command-mapping.md).
The prior accepted CPU arena and original v37 image retain their own scope.

The new stage option is `--intel-command-ownership`. Its separate image selects
wire6 of the original remapper/GGTT chain and an independent1,824byte version1
ownership packet. It uses the same exact cooperative EFI boot policy as v37;
the distinct source/image/release/wire are frozen by the workflow. Older wires
reject the packet, and the new profile requires it. Ordinary images and v37 do
not compile this adapter. No mapping or submission core is linked into it.

## Joined native observations

The original command hold registers this dependent before any new callback.
That blocks CPU close/return and the existing native parent-return gate. Every
group revalidates the original source/root/requester/HDA/UC mappings; each load
also has a same-origin clock admission. Header snapshots detect changes during
clock callbacks. All existing preparation150ms, GT300ms, HDA400ms and outer500ms
bounds remain, including final admission and parent return.

Each of two samples contains:

- nine actual graphics PCI requester/AF DWORDs, matching the held command3 and
  prior independently captured source, with no AF transaction pending;
- six ring/execlist DWORDs from each of five engine pages, matching the original
  post-reset source rather than a fabricated empty engine;
- thirteen programmed/live/stereo surface and control DWORDs from each pipe's
  admitted display page; the nine shared words match the original reader
  vector, all13 must remain equal between the two new samples;
- four exact GGTT entries at8704–8707, each sampled high/low/high and compared
  with the original complete16-entry window;
- a fresh CPU cache context, the four actual writable/supervisor/NX4KiB aliases,
  independent WB derivation and allocator-tag/direct-map backing checks.

There are180 sampled hardware DWORD reads in22 groups, plus162 independently
checked command-register admissions immediately before MMIO loads and two CPU
observations. PCI capability prefixes are validated before following each link.
No additional aliases or device-control/PTE stores are introduced. The original
remapper drain, System Agent invalidation, requester/engine controls and full
retirement still run and have their existing independent receipts. The new
packet does not alter the meaning/counts of older diagnostic packets.

The reader offsets follow the earlier
[pinned reader audit](intel-pinned-reader-audit.md); engine offsets follow the
[engine census](intel-engine-reader-handoff.md). Those guides pin the applicable
Intel Broadwell PRM sources. The PTE observations follow the existing window's
high/low/high convention. They are bounded observations, not atomic global GPU
snapshots or proof that firmware cannot submit later work.

## Proof boundary and retirement

The measured requirement mask is6: private RAM and actual CPU alias/cache
policy. Missing mask25 names exact GPU interval, GPU DMA/cache completion and
all-reader retirement. These are mapping-provider requirements, not the legacy
handoff assurance bits. All255 whole-device obligations and zero positive
handoff assurances/managed GPU pages remain. A stable PTE or reader snapshot,
reset-ready state, PCI BME clear, AF clear, CPU fence or remapper completion
cannot replace the missing witnesses.

Two matching samples may complete this observation scope. Only matching original
parent restoration and full CPU/backing/USB retirement can complete the whole
trial. A later hardware change can leave valid earlier observations while making
the enclosing trial fail. No partial receipt grants future live authority.
Unknown reads, clocks, registration, cache or source changes retain the dependent
and parent resources; there is no forced cleanup, fallback grant or retry.
Known missing requirements finish the dry run normally, unregister its dependent
and permit the existing verified parent return. CPU zero-preservation and
exactly-once alias/RAM release remain mandatory.

The strict receiver independently joins the packet to the original CPU lease,
PCI/engine/reader vectors, raw window entries, source epochs and native cache
context. It checks both sample envelopes, every original deadline and the exact
no-grant scope. Raw logs are unchanged. Hardware acceptance still requires this
unit's matching session, prior-file preservation, offline integrity, recovery
and consumed one-shot checks; host/VM results accept no native operation.

## Automated trial

Use the [device workflow](device-workflow.md) with profile
`intel-command-ownership-v38`, the local unit configuration and a clean committed
candidate. Qualification includes the previous profile's required gates plus
actual native adapter faults, strict packet/provenance mutations, fatal
sanitizers, compiled weakening controls, native2KiB frames and image isolation.
The dedicated ring and saved tail are96KiB so the added packet cannot evict
required boot provenance; ordinary images and older profiles remain64KiB. The
collector pins the96KiB bound to this frozen profile. Both default/diagnostic
ring wrap/framing and full-capacity workspace write/readback are tested.
The new graphics adapter retains its2KiB frame check. The existing generic
logger's larger telemetry frame is separately checked below36KiB in both modes;
the additional ring/save storage is static, outside the GPU critical lifetime.
Both exact-stage VM save/return and missing-workspace cases must pass. Q35
refuses the native profile; that refusal is not native ownership evidence.

The guest prints progress and needs no input. Volume is1% and no sound is played.
Safe completed observations save/unmount and return automatically to Ubuntu.
Uncertain retained state halts visibly; missing completion is not a pass. Keep
Ubuntu default, the known-good current payload and independent recovery intact.
