# Broadwell BCS reset readiness

This is the next bounded hardware step toward a controlled engine-reset path.
The [accepted completion audit](intel-completion-prerequisites.md) observes empty
queues and parser idle. Those observations do not establish write/prefetch
completion. A command-flush path first needs a private ring/context, completion
backing and maintained translation/cache ownership. This diagnostic instead
qualifies the reset request/ready/cancel handshake, without performing a reset.

## Register contract and limits

Intel's [Broadwell PRM Volume 2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
printed page 165, defines BCS_RESET_CTRL at `0x220d0`: request bit 0, ready bit 1,
low bits 15:2 reserved, upper 16 write masks. The diagnostic writes only
`0x00010001` to request readiness and `0x00010000` to cancel that request. It
observes readiness then cancellation; it never writes the ready status bit.
Raw upper mask halves are retained as observations, not compared as active
request/status state; admission, acknowledgement and cancellation use the
defined low half. A mask-change fixture checks this distinction.
The [Linux v6.12 reset implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
provides an independent reference for masked request/cancel ordering. Its retry,
catastrophic-error and forced-reset paths are deliberately not imported.

No `GDRST` store is present. GDRST completion and a ready acknowledgement are
not a documented system-memory/cache drain fence in this contract. No Stop Ring,
TLB invalidation, GPU command, cache flush, PTE or framebuffer change is added.
The obligations mask stays 255. Reset readiness grants no posted/prefetch,
pipeline, firmware, reader, cache, translation or backing retirement.

## Lifetime and admission

A dedicated `intel-reset-ready` image and firmware request require the exact
15-token recipe, adding `srvros.intel-reset-ready=request-cancel` to the existing
completion audit. Old recipes/images cannot enter it. Ordinary desktop boot and
other machine profiles are unchanged. All nine aliases/cache checks are prepared
before the first wake store; only the wake request page and exact BCS page are
writable. Both are NX/UC; other engine and TLB pages remain read-only.

A complete, same-parent, address-sealed completion census must precede entry,
with no enabled/pending/saved-flip-wait/TLB-pending masks and parser-idle mask 3.
A fresh exact BCS snapshot requires zero tail/head/start/control, execlist 1/0,
MI_MODE 512 and BLT_MODE zero, bound to that census. Existing reset control must
be clear. These narrow admission values match Air7,2's captured state; sibling
models are not enabled by similarity.

The dependent holds a one-use address-sealed GT borrow registered with its live
parent. Parent close is exercised and must refuse without issuing a store while
the borrow is live. Copied, stale, forged, mismatched and reused borrows cannot
return a pin. Clean cancellation and unchanged post-state permit return. Any
possibly issued request followed by an uncertain store, missing acknowledgement,
clock/guard failure while live, state drift or unsafe cancellation retains the borrow,
requester/mappings and CPU exclusion until reset. There is no forced close,
blind retry or cleanup store after such a fault. A sticky GT retained-state
query also blocks the main automatic-return gate, alongside the existing HDA
check; the pre-entry warning remains visible without bypassing display exclusion.

Ready and cancellation each have a 10 ms/256-poll bound; the whole dependent
has a 100 ms bound including verified borrow return, inside the unchanged 300 ms GT/500 ms outer lifetime.
Failure does not increase a timeout. A late but verified clean return is
incomplete timing evidence; it does not invent retained hardware ownership. Raw BCS rows and all four control endpoints,
read/poll/write counts, timing, blocked close and borrow/return status are saved.
The wake receipt is version 5; versions 1–4 keep their prior counts and meaning.
The collector binds completion, dependent and native/UC/checkpoint evidence;
a successful dependent under a failed outer lifetime remains incomplete.

## Next hardware trial

```sh
make -j2 all test-intel-reset-ready build/intel-reset-ready/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-reset-ready-trial
python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Follow the actual unit's [Ubuntu workflow](ubuntu-device-agent-setup.md) and
[Air7,2 record](machines/macbookair7-2.md). Progress/checkpoint/results appear in
the guest; no human input is needed. Volume 1%, audio disabled. Safe handled
outcomes save/unmount and return automatically; retained state or unsafe
persistence halts visibly. Q35 must refuse before any device store and preserve
its diagnostic outcome. Host/VM success does not accept a physical handshake.

After native readiness, qualify a precisely scoped reset's domain/side effects
and outstanding-memory guarantees before attempting it. Keep all firmware and
inherited display allocations protected. Actual write/prefetch completion, GPU
pipeline flush, maintained translation invalidation and CPU/GPU cache ordering
must be established before transferring backing or enabling page flips.

## Validated Air7,2 candidate

Source `a54e2ebd`, release `46037d20cb190504`, is installed and not armed; native
readiness remains pending. The [unit readiness record](machines/macbookair7-2.md#bcs-reset-readiness-ready-for-hardware)
records72 current-source validation jobs, fatal33 core/32 adapter sanitizer cases,
13-image isolation,64 detected compiled controls (including2 actual native MMIO/
mapping-table controls), exact-stage Q35 and61,553-byte conservative log projection.
Unchanged protocol evidence retains its original source and verified log hashes;
full host/core and changed dependent tests rerun. The new upper-mask fixture,
late clean-return deadline and retained-GT automatic-return gate are covered.
Media/recovery/default and check-only reboot guards pass; volume1%, audio off,
no human input required. Hardware readiness, actual reset/completion and cache/
backing ownership remain unaccepted. No reboot is armed by this checkpoint.
