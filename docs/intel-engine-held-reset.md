# BCS reset inside maintained engine preparation

This separate diagnostic composes the original five-engine preparation borrow,
the [held requester observation](intel-held-requester.md) and the reviewed
[BCS8 reset sequence](intel-bcs-reset.md). It does not turn the read-only action
into a reset callback. Ordinary desktop startup and older diagnostic images
cannot call this reset-capable entry point.

## One owner, two epoch meanings

`intel_engine_prepare_with_bcs_reset` requires an original zero-initialized
preparation and reset result. The original preflight and full census retain
their source epoch; the preparation's `epoch` never changes to disguise stale
evidence. A separate address-sealed reset context binds the original preparation,
parent, sole registered borrow and expected current epoch.

The sequence is:

1. Validate the same-parent source census/preflight, borrow the awake pinned
   parent, reject early close and take the initial reset-control snapshot.
2. Request all five engines in order and verify every ready acknowledgement.
   Capture a held snapshot, run the read-only PCI action under that same borrow,
   then capture and compare a second all-ready snapshot.
3. Enter the distinct reset scope. Independently validate full backing/endpoint
   state and capture fresh BCS geometry/context plus GDRST0. The eight BCS words
   must equal the source census and exact empty profile0/0/0/0/1/0/512/0.
4. Advance the parent's epoch exactly once before the first possible GDRST
   store, even if the provider subsequently guarantees no issue. Issue only
   GDRST8, wait for hardware self-clear, repeat the fixed successful pulse, then
   measure at least50us of settling. A failed or uncertain first pulse is never
   retried. Software never clears GDRST to cancel it.
5. Capture and compare fresh BCS geometry/GDRST0 and independently validate
   full provider exit. Verify a new mixed reset-control snapshot: BCS low0,
   other four still low3, GDRST0. Retain the earlier held snapshots separately.
6. Cancel all five requests in reverse order, including explicit BCS masked
   cancellation. Verify final clear controls, a fresh full census at the advanced
   epoch, unchanged92 raw engine/context DWORDs, both display/HDA snapshots and
   verified borrow return. The outer owner still must close and retire normally.

Broadwell PRM Volume2c, printed pages165 and609–610, defines BCS request/ready
clearing on engine reset and GDRST bit3's self-clear/non-posted behavior. The
pinned Linux v6.12 prepare/reset/cancel path supplies the two-pulse and50us
settling ordering; links and reference hashes are in the reset contract. Holding
other engines does not widen the reset domain. Any changed non-BCS hold or raw
preservation mismatch refuses completion.

## Bounds and retained failure

The existing preparation100ms origin covers everything, including requester
capture, reset, settling, cancellation, preservation and verified return. The
reset helper's own begin timestamp is diagnostic, never a new allowance. Each
pulse retains begin/end clocks under the unchanged10ms/256-observation phase
bound. Parent and outer bounds remain300/500ms. Overflow, copied/stale objects,
reentry, lost provider/clock/lifetime or foreign GDRST bits refuse.

The helper seals entry before its first callback. It does not create or return
a second borrow. Full entry and exit bracket one synchronous reset scope;
fresh original-lifetime, alias, requester and shared-clock admission remains at
every access. Native admission checks the clock immediately before the exact
DWORD store. Known completed loads/stores survive failed exit validation; raw
bad values and simultaneous postguard failure retain both causes.

Any failed operation after preparation stores retains the original pin and
exclusion. There is no forced cancellation, epoch rollback, optimistic restore
or retry through uncertainty. Missing completion is a nonpass even if reset ACKs
or later snapshots appear plausible. A completed helper alone does not accept
the preparation, owner or outer lifetime.

## Isolated native recipe and receipts

`build/intel-held-reset/srvros.elf` has its own firmware request, wake version17,
and exact18-token recipe: the held requester recipe plus
`srvros.intel-bcs-reset=held-blitter-only`. Only this new image makes GDRST
slot9 writable in addition to the existing wake and five engine-control pages.
The writer admits only0x941c/value8/DWORD. No PCI configuration or PTE writer,
broader domain, enabled-ring shutdown or graphics memory grant is added.

Preparation summary version2 binds the source and final epoch, actual reset
write count and five ordered snapshots. Older preparation version1 and wake
versions keep their exact behavior. `intel-held-reset-row` retains both ordered
BCS8/GDRST prefixes, including partial counts. Each completed store/first-ACK
has an `intel-held-reset-ack` with first/final values, extra polls and phase clocks.
The summary binds original origin, epochs, provider entry/exit, settling, first
failed clock, stage, counts and incomplete/uncertain state. Strict collection
requires all raw relationships, the preceding read-only action, matching fresh
preservation and clean outer retirement for native completion.

The new image losslessly packs each13-DWORD firmware snapshot as a fixed-width
hexadecimal vector; fixed offsets and all raw words remain available to the
existing firmware checks. Older images retain their word receipts. The saved
tail and its reserve must be qualified before staging; truncation is a failure.

```sh
make -j2 test-intel-held-reset build/intel-held-reset/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-held-reset
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Follow the [Ubuntu exact-stage workflow](ubuntu-device-agent-setup.md). The trial
needs no input, displays progress and saves its result. Safe saved/unmounted
outcomes return to Ubuntu; uncertain ownership or failed persistence halts
visibly. Keep volume1%, retained recovery and independent evidence backups.
Host/VM evidence does not establish native timing or preservation.

All eight memory/completion obligations remain open (mask255). BME/TP samples,
five ready requests, BCS reset acknowledgement and raw preservation do not prove
posted-write, prefetch, cache, translation, firmware or reader retirement. No
GGTT interval, private backing or managed page is transferred. Physical
tear-free presentation remains a later milestone in the
[roadmap](intel-tearfree-roadmap.md).


## Exact candidate qualification

Payload source `d22194cd` (protocol `5827e45d`), release `a25250e9a1f62f9e`,
is qualified using checkout `41bb435f`. That sole harness-only dispatch change
leaves the content-addressed payload and its original provenance unchanged;
the local source-projection proof binds the exact helper hash. Full local gate
(installed-root omitted),21-image isolation,486 fatal sanitizer cases,
eight core/14 native compiled controls and fresh staged instruction/table review
pass. Strict receipt/legacy checks retain their meanings. All92 post-census and
26 firmware DWORDs match the older raw decoder. Worst-width projection65,028
includes the unchanged1,536 reserve within65,536 bytes.

Exact-stage MBR EFI Q35 refusal/save/unmount/automatic return and missing-drive
halt pass; four saved logs match the compiled18-token recipe. USB integrity,
independent payload/recovery verification and check-only reboot guards pass.
Installed at1%, Ubuntu default, no reboot armed. Independent evidence survives
distclean. Native timing, mixed controls, preservation and retirement still need
the matching physical session. Separate earlier resets/captures do not accept
this composition or any GPU-memory obligation.
