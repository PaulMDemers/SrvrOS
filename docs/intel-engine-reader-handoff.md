# Broadwell engine readers and managed GPU memory

The accepted Air7,2 cooperative wake lease permits a bounded GT register access
window. It does not establish retirement of graphics engines or other memory
readers. This next explicit diagnostic observes command-streamer state while the
owned wake request is held. It cannot stop engines, submit work, rewrite a GPU
page table or supply managed-range assurances.

## Hardware sources and exact register set

Intel's [Broadwell PRM index](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/broadwell.html)
links [Volume2c, November2015](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`.
Printed pages435–437 define the64-bit read-only execlist status; pages1345–1353
define ring control, head, start and tail. The exact five page bases are
`0x2000` RCS, `0x12000` VCS0, `0x1a000` VECS, `0x1c000` VCS1 and `0x22000`
BCS. Each pass reads six DWORDs at offsets `0x30`, `0x34`, `0x38`, `0x3c`,
`0x234`, `0x238`. There are no clearing/status-acknowledgement reads, ring
submission, stop, reset or control writes. This remains exact Air7,2/1626 profile
work; register presence is not generalized to Haswell or another GPU.

Two passes preserve all60 raw DWORDs. `enabled` is the union of ring-control bit0
across the passes. `pending` marks differing masked tail/head offsets or sampled
execlist valid/full/active-element bits. `stable` compares every word. These are
diagnostic bitsets, not complete activity or ownership proofs. The two execlist
DWORDs are separate reads, not an atomic64-bit snapshot; quiet results do not
establish the absence of prefetched work, outstanding memory transactions or
later firmware submissions. Active or changed state is a valid observation,
never a reason to silently disable an engine or grant its addresses.

## Isolated implementation

`--intel-engine-capture` stages `build/intel-engines/srvros.elf` with its own
version1 recipe. All13 tokens, including `srvros.intel-engines=read-only`, must
match exactly. The accepted12-token wake-only image/recipe stays separate and
cannot read engine pages. Normal/AP/AML images omit the native wake and census
entry points. The main capture still verifies a durable `session-a.log` before
any store and drains periodic logging before IRQ-masked entry.

Build and stage using this unit's actual volume serial:

```sh
make -j2 all test-intel-engines build/intel-engines/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_VOLUME --intel-engine-capture
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Match the physical partition table; use `gpt` for a GPT workspace. Follow the
[Ubuntu device-agent guide](ubuntu-device-agent-setup.md) for durable payloads,
recovery, explicit-release installation, saved run, one-shot boot and collection.

After fresh native firmware/requester prerequisites and CPU exclusion, prepare
all eight aliases before the first wake store. The first three retain the
accepted request/ACK/C0 mapping contract; the five engine pages are RO/NX/UC.
Every new page requires exact physical identity, reserved resource coverage,
fresh unchanged CPU cache context and independently derived MTRR/effective UC.
The exact page list is separate from the earlier observer allowlists.

The census core checks the owned lease on both sides, guards every page group with fresh owned-request/ACK/C0 reads,
and refuses zero/backward/frozen or over100ms census time. The unchanged wake
phase100ms/total300ms and full bracket500ms bounds still apply. Reads remain
bounded even if hardware never becomes quiet; there is no retry-until-idle loop.
A failed read can close a still-valid wake lease and report incomplete census.
Wake uncertainty keeps the existing mappings and exclusion until reset. Clean
closeout retires all eight aliases, repeats firmware/original-window/backing
checks and releases CPU exclusion.

The wake receipt is version2 for this image, with eight maps/cache checks,
three stores and a separate engine-read count. The engine receipt supplies
raw rows, derived bits, timing, completed reads and retirement. Native acceptance
requires both receipts, exact native prerequisites, the saved preflight, loader
identity, independent UC derivation and timing order. No VM result accepts the
native engine registers. Q35 must refuse before stores and save/unmount/return;
missing workspace still halts visibly.

## Managed GPU memory roadmap

The existing `scanout_space`, `scanout_handoff`, lifetime and publication cores
provide address/physical-buffer leases, protected inherited ranges, quarantine,
restore and reader-retirement contracts. The native16-page candidate remains
populated and unavailable. This census supplies **zero** `HANDOFF_*` assurances.
Keep all inherited mappings and the full fence protected.

Before a native range transfer:

1. Bind same-boot firmware/requester control and the engine state to a maintained
   ownership lifetime. If engines are active, implement the documented bounded
   takeover/retirement sequence; stable snapshots alone cannot replace it.
2. Audit primary/sprite/cursor display readers, pending flips and display-audio
   DMA independently. Preserve the current visible scanout and every inherited
   allocation. The existing display capture does not cover every sprite reader.
3. Bind VT-d/bus-master/other DMA ownership and CPU/GPU cache/translation rules.
   The earlier read-only DMA observation is not DMA authority.
4. Transfer a specific interval only after all assurances are maintained, using
   the existing arena/pin contracts. Allocate private physical backing separately;
   being outside the visible framebuffer is not evidence that an address is free.
5. Bind exact original-PTE preservation/restoration, documented invalidation and
   device-reader retirement before releasing either address or physical memory.
   Then integrate publication and native vblank flips.

The original physical test was this automated census, with volume1%, no audio or
human input, persisted progress and automatic return after handled outcomes.
It is an evidence step toward handoff, not a tear-free desktop acceptance.

## Validated candidate and native result

Air7,2 release `3d8c0e9688408a25` (source `531bc9dd`) passes the full local
host/core/profile/build gate, fatal sanitizers including14 actual engine-adapter
modes, all267 collector/staging cases and nine detected compiled negative
controls. The exact MBR stage verifies the EFI64/loader/recipe/checkpoint path
and no-write Q35 refusal/save/unmount/return; missing workspace halts. Existing
Intel/Haswell and VirtIO regressions pass. Only the census ELF links the engine
observer. The unit record identifies installed recovery and independent backups.
Native session `00065fa38652c31d-00` now accepts the bounded read-only census:
both30-DWORD passes match,60 reads total, stable1, enabled16, pending0. Census
time is38,269us; the complete bracket is222,667us. All eight aliases/cache
checks retire, three wake stores complete,89 guards pass, no uncertainty remains,
and firmware/inherited-state preservation and CPU exclusion release verify. The
matching saved preflight and full native prerequisite chain pass; collector
attention0. Prior-file/media/recovery/automatic-return checks pass. No reboot
is armed; the trial needed no input and used volume1% with audio off.

RCS/VCS0/VECS/VCS1 ring controls are zero. BCS remains enabled with raw
start`0x03eef000`, control`0x0000f001`, head=tail0 and execlist low/high1/0 in
both passes. The start exceeds the inherited display-fence end`0x02200000`;
that display fence is not a complete firmware-allocation inventory. These
raw observations do not retire the ring or certify the absence of prefetched
work or other readers. Next is a documented bounded BCS retirement protocol and
remaining reader/translation/cache obligations before any native range transfer.
No GPU PTE writer or managed-memory assurance is enabled.

The next separate [idle BCS disable and reader trial](intel-blitter-retirement.md)
adds one guarded front-end control transition under a new explicit boot policy.
It cannot promote these observations into general engine/reader or memory
ownership.
