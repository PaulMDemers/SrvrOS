# PCI requester capture during maintained engine preparation

This is a read-only software composition of the
[preparation action](intel-engine-prepare.md#maintained-read-only-action-boundary)
and the existing [PCI AF profile](intel-pci-requester.md). The initial software
core has no configuration writer or GPU memory grant; the isolated provider
follow-up below has its own recipe and qualification. Acceptance of
the standalone requester capture does not accept this combined lifetime.

## One preparation borrow

`intel_pci_held_observe` consumes an original zero-initialized result during the
preparation callback. It binds the original preparation, parent, epoch and sole
active registered borrow. The preparation's transient action-active state must
remain set; copied, cancelled, completed or stale preparations refuse. The core
preparation validator owns the exact takeover/census and held-state checks.
The sampler never registers, returns or substitutes a borrow. Success leaves
the preparation responsible for its second held snapshot, reverse cancellation,
full preservation and verified return.

The sampler captures the preparation's original deadline origin. Its own start
time is diagnostic only: reads and all surrounding validation must fit within
the preparation's existing100ms budget, including the work already performed.
No fresh100ms allowance is created for the action. The provider must use the
same monotonic timebase as preparation. Provider callbacks must be
bounded; software clocks cannot interrupt a hung hardware access.

## Exact read sequence and failures

Two samples read DWORD offsets0,4,8,2c,34,90,d0,a4,a8, using the existing
Broadwell GPU/MSI/PM/AF profile. The captured prefix is checked before each next
fixed access. Malformed identities, links, AF layout or reserved status bits
prevent subsequent reads; no arbitrary capability pointer is followed. Provider
guards and live-owner/clock checks surround individual loads. A successful load
is counted before checking its value or exit validation, so a malformed word or
lost exit retains the completed raw prefix. Failed reads are attempted accesses,
not fabricated completed words.

The result retains raw rows, counts, full-profile masks, BME/TP/FLR observations,
raw stability, completion and first failed clock diagnostics. Completion is
historical capture evidence, never reusable authority for a live hold. A valid TP change
can complete capture. TP clear, BME clear or raw stability is not a requirement
for a valid observation and is not comprehensive DMA completion.

The caller must propagate sampler failure from the read-only action. Earlier
preparation request stores already established uncertainty; failed callback or
lost action/parent/guard/clock state does not authorize forced cancellation,
returning the pin or freeing aliases. This is a trusted kernel callback contract,
not isolation against arbitrary writes to the preparation object.

## Native integration remains separate

The initial software-only source `f0b66b22` has no native caller. The isolated
provider follow-up must validate the
actual device/firmware/resources, exact current mappings and requester state;
it must not promote host callbacks into positive hardware assurances. It also
needs its own exact recipe, progress, raw receipts and strict collector before
any combined physical trial. Existing installed/recovery payloads are unchanged.

Standalone preparation and requester timings cannot simply be added or treated
as a pass. Their previous93,246us and27,036us measurements show why the entire
combined lifetime must be measured and qualified within the unchanged bound.
Any optimization must preserve the independent admission and exit checks.
Another observation-only hardware trial is justified only by a specific
composition question and a qualified bounded provider.

No reset, BME/FLR/PTE write, maintained firmware/DMA/cache/translation/reader
ownership or GGTT interval transfer follows. All eight completion obligations
remain open. A reset-capable composition needs a separate epoch/control-state
protocol; this read-only action cannot supply it. The
[tear-free roadmap](intel-tearfree-roadmap.md) orders the remaining work.

## Local qualification

The actual preparation/action/sampler cores pass158 directed cases under fatal
ASan/UBSan. These include every attempted read boundary, malformed/floating
prefixes, copied/stale preparations/results, reentry, final-clock lifetime loss,
expired shared budgets and combined read/profile failure with provider exit
loss. Five compiled controls detect missing original-action validation, a fresh
sampler budget, skipped exit validation, missing final-live checks and missing
loaded-prefix validation. A weak initial deadline-test assertion was strengthened
after its control survived; the corrected test requires stopping on the first
expired read even though the sampler's own elapsed time is below100ms.

Existing action/preparation/requester suites, host checks, ordinary build and
isolated no-action preparation image build pass. Symbol checks show the held
sampler absent from both linked images; its compiled core imports only the
preparation validator and existing PCI prefix/offset definitions, with no borrow
management or writer imports. Documentation checks pass. Evidence remains local
and independently backed up outside build. There is no native/VM acceptance of
this sampler and no staged or installed combined candidate.

## Isolated native provider and receipts

The follow-up adds a distinct `build/intel-held-requester/srvros.elf`, firmware
request, wake version16 and exact17-token recipe. It combines the existing
preparation recipe with `srvros.intel-held-requester=read-only`; it is incompatible
with the standalone requester/reset/AML/AP profiles. The ordinary desktop and
older diagnostic image recipes remain unchanged. Native acceptance is pending.

One synchronous action has full provider entry and exit backing/endpoint checks.
Every individual load also validates the current aliases, fresh requester state,
original action/parent and shared clock; the native provider checks the clock
again immediately before the DWORD PCI read. Successful admission is never
carried into another callback. Failed full exit is sticky, retains the completed
raw prefix and invalidates capture completion. Receipts explicitly retain full
provider entry/exit witnesses. No new hardware writer or mapping permission is
added; existing five-engine request/cancel stores remain the only engine action.

`intel-held-requester-row` retains two ordered raw PCI prefixes, and its summary
binds the original preparation origin, epoch, clocks and derived masks.
`intel-prepare-action` binds callback time, completion, inactive final state and
both provider witnesses. Successful overall completion requires all of these,
matching held snapshots, reverse cancellation, full preservation and clean return.
A failed clock may leave the action's last valid timestamp before the sampler's;
the collector accepts that only as incomplete retained evidence with matching
expired-clock witnesses and no cancellation/return/release.

Only the new image uses `intel-prepare-post-vector: v=3 s=... n=... h=...` for
post-census data. Each raw DWORD occupies exactly eight lowercase hexadecimal
digits: five ordered eight-word engine groups, then six context words. Nonempty
prefix lengths are8/16/24/32/40/46; all92 full-census DWORDs survive. Strict decoding
normalizes these into the existing preservation checks. Wrong count, mixed or
older format, uppercase, duplicates and reordering refuse. Initial census and
all older image formats remain unchanged. This lossless compaction preserves
the fixed65,536-byte log and its1,536-byte reserve.

```sh
make -j2 all test-intel-held-requester build/intel-held-requester/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-held-requester
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

The [Ubuntu workflow](ubuntu-device-agent-setup.md) still requires exact local
identity, immutable staging, integrity and independent recovery. The in-guest
trial displays progress and needs no input; safe saved/unmounted outcomes return
automatically, while uncertainty or persistence failure halts visibly. Keep1%
volume. VM refusal/persistence cannot accept native timing, simultaneous held
requester state or any memory/completion obligation.

## Exact candidate qualification

Source `8559ac44`, release `67a178af16c8983f`, is locally qualified and installed
unarmed at1%. The full local gate (installed-root omitted),20-image isolation,
116 fatal provider sanitizer cases, four compiled controls, strict collection,
legacy classification and actual-object review pass. Exact-stage MBR EFI VM
refusal/save/unmount/return and missing-workspace halt pass; four saved logs
match the compiled17-token recipe. The raw-vector comparison retains exactly
92DWORDs. Worst-width projection65,485/65,536 includes the unchanged1,536 reserve.
USB contents/integrity and recovery verify; evidence survives distclean outside
build. This accepts local mechanisms/refusal/persistence only. Native combined
timing, preservation and retirement are pending the matching hardware session.
