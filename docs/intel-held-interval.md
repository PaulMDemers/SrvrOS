# Original GGTT entry inside the held reset scope

The separate `--intel-held-interval` image binds one candidate GGTT entry to the
original active reset/preparation borrow. It observes the entry after the BCS8
epoch transition and before cancellation, preservation and pin return. This
addresses a lifetime gap: a completed reset receipt cannot serve as a live
owner for a later memory operation. It does not transfer a GPU range or add a
PTE writer. The [complete transfer mechanism](scanout-transfer.md) still has no
native provider with positive hardware assurances.

The BCS reset clears its control state. An active software preparation/reset
borrow is not proof that all five hardware ready requests remain asserted after
reset, nor that DMA/cache/readers have retired. This observer makes neither claim.

## Exact scope

The existing exact Air7,2 window/profile/cache/firmware checks remain in force.
The new image retains the same thirteen control/display aliases and adds only
the already observed candidate page at MMIO offset `0x811000`. Its alias is
read-only, NX and UC, mapped before the critical preparation section and kept
until the parent is safe to retire. No aperture data mapping is made.

The new observer runs in the reset provider's exit callback, after both BCS8
pulses, settling and the post-reset snapshot. It requires the original reset,
preparation, current parent epoch and sole registered active preparation borrow.
It neither acquires a second borrow nor returns the parent's dependency. It
reads two high/low/high DWORD samples at offsets `0x811004`, `0x811000`,
`0x811004`, comparing every completed word with the previously captured first
candidate entry. Prefix drift or a lost source stops further loads. Fresh
source/alias/requester guards bracket each load, and the native adapter checks
the original deadline immediately before its DWORD access.

All clocks use the preparation's original 100 ms origin. The separate GT-owner
300 ms and outer 500 ms limits stay unchanged. Neither a new helper nor a later
sample restarts any budget. The previous accepted combined preparation had only
2,586 us margin in one run; native timing for this extra observer is pending.

The result is address-sealed and one-use. Its manifest export works only while
that exact post-reset source is still active, and keeps its original data in the
original object. It carries **zero hardware assurances and all 255 open
completion obligations**. The transfer core therefore cannot import it. After
the enclosing reset/borrow completes or fails, the export refuses. Copies,
wrong epochs and returned or detached borrows cannot serve as a current source.

## Saved proof and failure handling

The v18 wake receipt binds fourteen maps/retirements and the six additional
DWORD loads. Two `intel-held-interval-row` records retain raw completed prefixes
and sample clocks; the summary binds the original origin and before/after
epochs. A completed observer is separate from completion of the enclosing
reset, preparation, preservation and wake lifetime. Collection requires all of
them, matching original window data and the saved prerequisite checkpoint.
Modeled hardware fixtures cannot satisfy native backing prerequisites.

To keep the existing 65,536-byte log tail, this image packs all 74 plane/HDA
preservation DWORDs into one canonical base64 record:
`intel-prepare-preservation: v=3 n=74 b=...`. Each complete sample contains
three nine-DWORD plane records followed by ten audio DWORDs. Counts 0, 37 and
74 describe complete sample prefixes; no raw value is omitted. Decoding restores
the existing plane/audio schema and checks exact equality and native identities.
Older images retain their existing formats; v18 requires the packed format and
rejects mixed or duplicate records. The log reserve and all deadlines stay fixed.

Before native accesses, the exact source saves its original display/backing/
window/firmware prerequisite checkpoint. The in-guest progress message requires
no input. A failed observer makes the enclosing provider fail; uncertainty keeps
the original borrow, aliases and exclusion pinned. Failed persistence or retained
uncertainty halts visibly for a photograph. Safely saved/unmounted outcomes
return to Ubuntu automatically. Keep volume at 1% and Ubuntu/recovery protected.

## Isolation and qualification

The new image has its own firmware request and exact 19-token recipe, adding
only `srvros.intel-interval=read-only` to the held-reset recipe. Only this image
defers user system-init for that recipe, preserving the original sole-root
admission predicate. Ordinary and older images keep their prior startup and
recipe rules. Missing, duplicate and foreign tokens refuse; do not enable AML,
APs or GUI clients in this diagnostic.

```sh
make -j2 test-intel-held-interval test-intel-held-reset build/intel-held-interval/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL --intel-held-interval
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

Use the local unit's UUID. The actual-core fixture runs GT/census/takeover/
preparation/reset and observer together; it does not hand-build a live reset
token. Faults cover every observer clock, guard and load, copied/reentered
results, mutable inputs and both DWORD halves. Production-adapter fixtures
check alias permissions and read order, admission clocks, lost requesters/maps,
entry drift and retained failure. Receipt tests reject malformed, missing,
reordered, spliced and false-authority records. Full qualification also requires
image/object isolation, sanitizers, compiled controls, log-budget checks and
exact-stage VM persistence/refusal and missing-workspace paths.

Native acceptance remains pending in the [Air7,2 record](machines/macbookair7-2.md).
Even a successful hardware round accepts source binding and timing only.
Exclusive interval transfer, GPU encoding, posted-write/invalidation completion,
cache/reader retirement and display publication remain separate work in the
[managed-memory plan](intel-managed-memory-plan.md).

Source `3315df79` is locally qualified in exact release `e07848a7c00d893d`:
23-image isolation, 251 fatal sanitizer cases and 14 compiled fault controls,
including two native-object controls, pass. The native load uses the original
100 ms origin, with its timer check immediately before the DWORD access. All
74 preservation DWORDs compare losslessly against the older actual C producer.
The projected log remains 65,371/65,536 bytes including the unchanged 1,536-byte
reserve. Exact-stage QEMU verifies unsupported-hardware refusal, persistence,
unmount, automatic safe return and missing-workspace halt. The candidate is
installed with recovery protected and no reboot armed; native timing and source
binding still require the machine's matching returned session. The newer PR457
exFAT changes are deliberately outside this frozen qualification source.
