# Guarded idle blitter shutdown qualification

The Air7,2 reset trial on source `9ea439f6` correctly refused its inherited
BCS ring at enabled mask16/control `0xf001`. The reset core requires a disabled
zero-geometry entry. Clearing enable preserves the captured start `0x03eef000`
and length/control `0xf000`; these are different states. This separate milestone
qualifies shutdown before changing reset admission. It performs no reset and
never clears the ring start, releases backing or claims DMA/cache retirement.

## Separate authority and image

`--intel-shutdown-trial` stages `build/intel-shutdown/srvros.elf` with its own
firmware request and exact15-token recipe. It adds
`srvros.intel-shutdown=qualify` to the existing blitter recipe. The older
[blitter diagnostic](intel-blitter-retirement.md), [reset image](intel-bcs-reset.md),
read-only observers, ordinary desktop and Air6,1 sampler retain their recipes.
The explicit cooperative EFI requester and BCS submission handoff policies,
exact captured Air7,2 profile, CPU0/IRQ exclusion, fresh backing/resource/cache
validation and committed preflight remain mandatory. Opaque SMM is not excluded.

All11 NX/effective-UC aliases are prepared before wake. Only the requester and
BCS control pages are writable; no GDRST, HDA-MMIO, PTE, translation or flush
writer is present. The optional guarded requester snapshot retains fresh full
core pre/post and provider admission for each check. Raw values are never cached.

## Same-lifetime sequence

1. Read two stable five-engine censuses. Only the captured enabled16/pending0
   or exact zero-geometry disabled0/pending0 branch may continue.
2. Borrow a registered, address-sealed parent wake pin. Verify an early parent
   close is blocked without a store, then read the first27 plane DWORDs and HDA
   config. Bind primary/cursor state to the original inherited display and
   require the captured display-HDA identity and a complete config traversal.
3. Invoke the existing one-shot blitter core. Enabled entry requires two matching
   eight-DWORD BCS snapshots and the exact idle legacy geometry/mode; it permits
   one `0xf000` store to `0x2203c`, followed by two matching readbacks. The disabled
   branch requires a NULL writer and performs four exact zero-geometry reads.
   Before a possible control store, increment the owner's engine epoch. No retry,
   restore, Stop Ring or reset is available.
4. Read a fresh second five-engine census and the second plane/HDA pass. All
   streamer words must match the first census except the BCS enable bit; the
   fresh BCS words also bind the core's two final snapshots. Require no enabled
   engine or sampled pending queue. All54 plane DWORDs and all10 HDA config
   fields must agree, with inherited primary/cursor binding repeated.
5. Return the borrow only after mandatory postconditions. Close the requester,
   retire all11 aliases, revalidate firmware/backing and release exclusion.

The existing100ms per-census/core,150ms reader/shutdown bracket,100ms requester
phase,300ms owner and500ms outer bounds remain unchanged. No clock tie may
substitute for whole-operation progress. A definitely unissued store can return
its pin and report incomplete if the owner remains valid. Any possibly issued
disable followed by failed readback, post-census, reader, clock or ownership
checks retains the pin, mappings and exclusion until platform reset. No automatic
reboot may bypass retained state. Read-only failure still requires verified pin
return; invalid parent state remains retained.

## Receipts and collection

Wake version8 identifies this image; versions1–7 keep their meanings. Older row versions remain available for historical images. The new image uses
version2 decimal DWORD rows; compact engine rows encode the fixed page identity
by engine index, retaining all six raw words. This bounds full-width logs without
raising the existing64KiB tail capacity. `intel-shutdown-post-*` adds the
fresh60-word engine observation, and two `intel-shutdown-audio` rows retain the
four additional HDA fields beside the existing six-field config rows.
`intel-shutdown-summary` records pin/blocked-close/epoch/timing/completion facts.
The first-fault `intel-clock` row binds to the summary's owner origin. Strict
collectors require exact fields, versions, coverage, raw values, order, branch,
time bounds, epoch transition and native prerequisite/checkpoint/UC provenance.
A valid diagnostic refusal is incomplete, even when cleanup is clean.

Complete means a bounded verified command-front-end shutdown/observation and
same-lifetime preservation, including full software closeout. Reset/prefetch,
outstanding DMA, cache/translation ownership, managed GPU backing and physical
tear-free remain false. A successful trial will not accept the next boot's
reset or authorize reusing this boot's observations in another owner lifetime.

## Automated preparation

```sh
make -j2 all test-intel-shutdown build/intel-shutdown/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-shutdown-trial
python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the actual unit's partition table, recovery and UUID. Follow the
[Ubuntu guide](ubuntu-device-agent-setup.md) for immutable staging, installation,
one-shot boot, collection and independent evidence backup. No typing or playback
is required; volume1%. Safe outcomes save/unmount and return to Ubuntu. Retained
state keeps the pre-entry warning and requires manual restart. VM identity
refusal validates transport and collection only; native shutdown acceptance
is branch-specific in the [unit record](machines/macbookair7-2.md).

## Air7,2 readiness

Source `8efce479` / release `c64cd44f68ef1512` is installed, unarmed and ready
for native testing. Full local checks,36 fatal sanitizer cases,11 compiled host/
native controls,15-image isolation, exact-stage normal/missing-workspace rehearsal
and media/recovery checks pass. Full-width tail projection is64,974/65,536bytes.
An initial invalid generated recipe was caught before installation; actual
canonical command lines now run through the compiled kernel parser in regression
tests. Current immutable source/config/manifest binding is recorded in the
[unit checkpoint](machines/macbookair7-2.md#guarded-idle-blitter-shutdown-ready-for-hardware).
No physical shutdown or reset acceptance follows from VM/host evidence.

## Air7,2 physical result

Source `8efce479` / release `c64cd44f68ef1512`, session
`session-00065fbc5a35c396-00`, accepts **already-disabled observation and
same-lifetime preservation**. It made zero control attempts/stores, preserved
all engines,54 plane words and10 HDA config fields, returned its borrow and
retired all11 aliases. Owner259,797us and outer334,176us meet the unchanged
300/500ms budgets with no clock fault. Strict native/checkpoint collection and
prior-file/offline media/recovery checks pass; Ubuntu return consumed the one-shot.
The enabled-ring write remains untested. No reset, DMA/prefetch/cache/reader
retirement, managed GPU memory or tear-free capability follows. Any reset
integration must obtain fresh same-lifetime evidence; this boot's observations
cannot authorize a future boot's operation.

The [combined reset successor](intel-bcs-reset.md#fresh-disabled-observation-within-the-reset-borrow)
repeats only the exercised already-disabled observation inside the current reset
borrow. It accepts no prior boot receipt and does not import the untested enabled
control writer. Its distinct image/recipe and native acceptance are independent.
