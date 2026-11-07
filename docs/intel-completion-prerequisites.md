# Broadwell completion prerequisites

The accepted [pinned HDA/display-reader audit](intel-pinned-reader-audit.md)
observes non-posted TXP clear and stable plane surfaces. It does not complete
posted writes or prefetched GPU work. The isolated image qualifies the
remaining engine, saved flip-wait and translation/cache-control state under one
cooperative GT wake lifetime. All eight completion obligations remain open.

## Why a zero control is not a completion fence

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
defines BTCR/RTCR/M1TCR/M2TCR/VTCR on printed pages180,1327,973–974,1713.
Their bit0 requests TLB invalidation and self-clears when complete, but the
programming prerequisite is a pipeline already flushed and clear of memory
accesses. An initially clear bit, idle parser or empty sampled ring cannot
establish that prerequisite. This image never writes those controls.

The same manual's RCS INSTPM pages824–826 and BCS INSTPM page159 mark the old
invalidation/flush bit positions reserved on Broadwell. Do not transplant an
older-generation INSTPM sync-flush implementation. RCS GFX_MODE is0229c (page618),
not the older-generation02520 offset; bit13 configures invalidation behavior but
is not proof that a flush completed. BCS BLT_MODE is2229c (pages177–179).
The independent host adapter fixture checks every actual register offset against
a separately pinned list, including the different VCS1/VECS TLB order.

MI_MODE at0209c/2209c reports parser/arbiter idle for RCS and parser idle for BCS;
it does not flush the execution pipelines or prove posted writes drained. Saved
SYNC_FLIP_STATUS at each engine's base+2d0 is described on pages1793 onward.
The manual forbids software programming it. Nonzero saved wait flags are useful
observations, not an exhaustive live display flip-pending witness. There are
no status acknowledgements, Stop Ring, reset, pipeline/translation flushes,
PTE changes or completion-buffer submissions in this image.

## Exact observation and lifetime

The [engine pages](intel-engine-reader-handoff.md) stay RCS02000, VCS012000,
VECS1a000, VCS11c000 and BCS22000. Each of two passes reads the existing six
ring/execlist DWORDs, saved flip-wait flags and the associated TLB control. TLB
addresses in that engine order are04260/04264/04270/04268/0426c. Each pass also
reads six RCS/BCS context words at0209c/020c0/0229c/2209c/220c0/2229c. This is
80 group plus12 context reads,92 total. Raw DWORDs are preserved as unsigned
decimal. Floating words, reserved TLB bits, state changes and invalid/expired
clocks refuse completion; active/busy but stable states remain observations.

A separate exact RO/NX/UC page at GPU MMIO+04000 contains the TLB controls.
Earlier cache/observer allowlists are unchanged; a dedicated predicate requires
physical identity, reserved resource coverage and a fresh unchanged PAT/MTRR
snapshot with independently derived effective UC. All nine aliases are prepared
before the first existing GT wake store. Only the earlier wake-request page is
writable; engine and TLB pages are RO. Guards retain the owned request/ACK/C0,
CPU0/IRQ exclusion, root, mapping and native firmware/backing prerequisites.

The core consumes a zeroed, address-sealed result once and keeps the100ms census
bound inside the unchanged300ms wake/500ms full brackets. The existing wake
protocol has three masked request stores; this image adds no device-control
store. Failed read-only observation can close a still-valid parent and return
incomplete diagnostics. Ambiguous mappings, parent faults and unsafe retirement
retain mappings/wake/exclusion until reset. No partial data becomes acceptance.

The wake receipt is version4 with nine maps/cache checks and92 engine/context
reads. Older versions retain their own strict counts and native contracts. The
new receipt independently derives enabled/queued/saved-wait/TLB-pending/parser
idle masks, binds timing to that wake lifetime, validates full raw coverage and
fresh native/UC/checkpoint provenance. The obligations mask stays255: posted,
prefetch, pipeline, translation, cache, flip, firmware and backing. All authority
fields remain zero, regardless of the observed masks. No `scanout_handoff` grant
or memory reuse follows. The HDA inhibit and plane audits retain their independent
accepted lifetimes; combining separate boots cannot prove joint ownership.

The next isolated step is [BCS reset readiness](intel-reset-readiness.md), a
bounded request/ready/cancel handshake. It performs no actual reset or memory
completion, and keeps all eight obligations open.

## Next implementation boundary

Select a documented completion or controlled engine-reset path only after the
native state is known. A command path needs a driver-owned ring/context, private
completion backing and explicit ordering; it cannot reuse an arbitrary firmware
allocation. A reset path needs exact domain/side-effect and outstanding-memory
retirement guarantees while preserving the visible display. Then bind posted/
prefetch completion to maintained ownership before requesting TLB invalidation,
prove its completion and CPU/GPU cache ordering, and only then transfer a precise
backing/GGTT interval. PCI BME readback, CPU fences, arbitrary waits, parser idle,
saved flip flags and initially clear TLB bits do not replace these obligations.

## Automated trial

```sh
make -j2 all test-intel-completion build/intel-completion/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-completion-audit
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the actual machine/volume/partition table and [Ubuntu workflow](ubuntu-device-agent-setup.md).
The distinct completion firmware request/image requires the exact14-token
recipe, including the existing engine/wake tokens and the new explicit
`srvros.intel-completion=read-only`. Ordinary/old diagnostic recipes cannot enter
it. Persist the native prerequisite checkpoint before any wake store; pause
periodic logging before serialized entry. The guest shows progress and needs no
human input. Volume1%, audio off; save/unmount/return after safe handled outcomes,
halt on retained state or unsafe persistence. Q35 must refuse before stores,
save and return; missing workspace halts. These VM results cannot accept native
registers, completion, cache/engine ownership or physical tear-free presentation.

## Accepted Air7,2 observation

Release `6c2fffbf241a110a`, source `a24c164a`, completed native session
`session-00065fb6dba7b000-00` and returned automatically. Both samples agree:
all five engines have zero tail/head/start/control, execlist low/high 1/0,
saved flip-wait zero and TLB-control zero. RCS/BCS context words are
512/24704/10240/512/0/0 in the order above. Derived enabled/pending/flip-wait/
TLB-pending masks are zero; parser-idle mask is 3. These are sampled states,
not completion witnesses. All eight obligation bits remain open (255).

The 92-read observation takes 63,525 us; the surrounding wake lifetime takes
231,818 us, with nine retired aliases/cache checks, 93 guards, three existing
wake stores and retained=0. Bounds remain unchanged. Native/checkpoint/UC,
complete 58,470-byte saved exit, zero collector attention/errors, prior-file
preservation, offline media, recovery/default/consumed one-shot all pass. The
[unit checkpoint](machines/macbookair7-2.md#current-checkpoint-engineflip-waitcache-prerequisite-audit-accepted)
records evidence and independent backup. No engine/TLB/reset/cache command ran;
no posted/prefetch, pipeline, firmware, translation, backing or tear-free
acceptance follows. Separate HDA/plane trials do not establish joint ownership.

## Historical validated Air7,2 candidate

Before the trial, source `a24c164a`, release `6c2fffbf241a110a`, was installed
and not armed, with native acceptance pending. The [unit readiness record](machines/macbookair7-2.md#engineflip-waitcache-prerequisite-audit-ready-for-hardware)
identifies451 passing validation jobs,21 core/25 actual-adapter fatal sanitizer
modes,46 detected compiled negatives,282 collector/staging cases and12-image
isolation. Exact-stage Q35 verifies no-store refusal/save/unmount/return and
missing-workspace halt; prior Intel/Haswell/VirtIO regressions pass. Conservative
saved-tail projection is60,192/65,536 bytes including1,536 allowance. Recovery,
media and independent evidence are recorded there. No hardware completion or
memory authority follows from these host/VM receipts.
