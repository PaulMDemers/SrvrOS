# BCS reset inside maintained engine preparation

This separate diagnostic composes the original five-engine preparation borrow,
the [held requester observation](intel-held-requester.md) and the reviewed
[BCS8 reset sequence](intel-bcs-reset.md). It does not turn the read-only action
into a reset callback. Ordinary desktop startup and older diagnostic images
cannot call this reset-capable entry point.

## Native combined acceptance

Source`9c788f3f`, release`8d3839aaa57f707d`, session
`session-00065fe55d829da0-00` accepts one pre-user-init combined lifetime:
97,414/100,000us preparation,3,369us reset scope,14/32us pulses,51us settling,
10 request/cancel acknowledgements, five control snapshots and full92-word/
54-plane/two-HDA preservation. Source epoch0 stays immutable; parent advances
once to1. The original borrow returns,13 aliases retire and exclusion releases;
wake lifetime280,135/500,000us. No failure/clock/uncertainty/retained state occurs.
The2,586us preparation margin is one-run evidence only.

The generic collector's initial false attention on the valid zero-code admission
row is preserved. Fix`925ae934` decodes the full row rather than ignoring the
prefix; malformed/nonzero codes still attract attention. Reviewed collection
uses byte-identical raw files and the unchanged complete native/checkpoint/final
and integrity gates. No kernel rebuild or new physical run was needed. This
accepts the bounded composition, not GPU-memory transfer or native tear-free;
all255 completion obligations stay open. See the [unit record](machines/macbookair7-2.md).

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

## Fresh CPU translation comparison

This image alone uses `vmm_diagnostic_contiguous_kernel_range` at each full
backing endpoint while IRQs are masked. The query requires the current sole
kernel root, aligned canonical bounds and at most4096 pages. Every4K leaf is
read and compared; large leaves prove only the covered translation interval.
Ancestors are shared inside one uninterrupted call and rewalked at leaf-table
boundaries. Nothing is cached across endpoints or callbacks. IRQ-enabled
startup and older images keep their scalar path; a failed excluded query never
falls back. All original raw GGTT/fence/route/endpoint observations and
provider/lifetime/clock checks remain. No page-table mutation, alias or
permission proof, pin, DMA assurance or longer deadline is introduced.

Actual VMM tests compare scalar and batched translation, inject a missing or
wrong mapping at each of1350 framebuffer leaves, cross PT/PDPT/PML4 and large
leaf boundaries, and check exclusion and unchanged tables/state. The host
fixture observes5400 scalar table conversions versus11 batched conversions
for that range. These are lookup counts, not native wall-time acceptance.


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

## First native combined result: timing nonpass

Source `d22194cd`, release `a25250e9a1f62f9e`, session
`session-00065fe204ee8f63-00` records both BCS8 pulses completing in14us each,
51us settling and a3,539us helper scope. Mixed controls, reverse cancellation,
raw engine/display/HDA preservation and verified return all match. The complete
preparation nevertheless takes103,702/100,000us and remains incomplete with
reason2/clock at stage6. Its last valid guard is98,641us; the5,061us return tail
crosses the limit. This is a timing nonpass, not acceptance of the composition.

All13 aliases retire, with no uncertain/retained state; the outer wake bracket
is298,771/500,000us. The durable checkpoint, three new logs, clean automatic
return, prior-file preservation and offline filesystem/allocation checks verify.
No reboot remains armed. See the [machine result](machines/macbookair7-2.md)
for the next bounded timing investigation. Deadlines, admission checks and all255
outstanding completion obligations remain unchanged; no GPU memory is granted.

## Translation-cost retry candidate

Source `8b91de5b`, release `b63d89044865e7a8`, integrates main`b3c2f30d` and
uses the fresh CPU translation comparison described above. All full censuses
and original limits remain; no GPU register recipe or receipt version changes.
Full local gate,21-image isolation,486 existing fatal sanitizer cases plus the
actual-VMM sanitizer,8core/14native/5range compiled controls, native object/staged
bindings and strict historical receipt classifications pass. Exact-stage MBR
EFI refusal/save/unmount/automatic return and missing-drive halt pass; native
elapsed time still needs the matching physical trial. The earlier103,702us
nonpass remains unchanged evidence, never a pass under the retry's source.

## Native translation retry result: early admission refusal

Source `8b91de5b`, release `b63d89044865e7a8`, session
`session-00065fe36794a009-00` refuses IRQ-excluded inherited backing/window
revalidation before wake/reset access. Exclusion is acquired and released;
zero wake mappings, write attempts, writes, engine reads or retained state are
recorded. Preparation and reset never enter, so no timing improvement is
measured. The exact failed internal predicate still needs a bounded diagnostic;
the new range-query context is a hypothesis, not accepted native attribution.

The native checkpoint, three saved logs, prior files, offline filesystem and
recovery/automatic-return checks verify. Expected incomplete markers and
collector attention remain; no reboot is armed. All ownership checks, existing
deadlines and255 outstanding completion obligations remain unchanged. No GPU
memory or tear-free authority follows from this refusal or the host fixtures.

## First failed admission observation

The held-reset image retains one compact `intel-admission: v=1` row from the
IRQ-excluded wake lifetime. It is zeroed at entry, records the first failed
backing/window predicate and never overwrites it during later cleanup. Query
attempts are counted only in that lifetime. Printing happens after IRQ state
restoration. There is no extra target read, clock sample, table walk, allocation,
retry, fallback or writer in this instrumentation.

The row contains `queries code detail pages phase reason level checked value`.
Code0 means no failure recorded, not admission or ownership. Code7 binds the
original failed `vmm_diagnostic_contiguous_kernel_range_observe` observation;
`pages` is that query's input count. Other codes carry no query witness.
Earlier releases may omit the row; the matching new trial collector requires
it, without changing older receipt classifications or wake17 semantics.

| Code | First failed stage |
| --- | --- |
| 1–6 | Backing identity, host bridge, framebuffer layout/translation, memory catalog or overlap |
| 7 | Original CPU range query, with the exact phase/reason/prefix below |
| 100–104 | Revalidation adapter, backing core status, endpoint/count, raw PTE or fence comparison |
| 200–204 | Window adapter, cache-page prerequisites, UC check or endpoint |
| 300–304 | Original handoff snapshot, entry comparison, window core status, preservation or final entry comparison |

Query phase0 is input refusal (`value`1=count,2=VA alignment,3=PA encoding,
4=VA overflow,5=PA overflow,6=canonical bounds,7=canonical-half crossing).
Phase1 is the entry context and phase3 the exit context: reason1=IF set,
2=invalid root,3=kernel-root mismatch,4=current-CR3 mismatch,5=missing PML4,
6=PML4 pointer mismatch,7=registered-root count,8=occupied registry. These are
the original short-circuit checks in their original order, with no later
resampling to infer a reason. `value` retains only the observed IF bit, count
or occupancy for those reasons; no root/table pointer is exported.

Phase2 is the walk: reason10/11=PML4 absent/large,12=invalid table pointer,
13=PDPT absent,14=PD absent,15=PTE absent,16=PTE address mismatch,17=large-leaf
address mismatch. `level`1..4 is PML4..PT (`0` for large-leaf address mismatch),
and `checked` is the exact completed page prefix, never a hardware-completion
claim. A successful observed query internally reports phase4 and the complete
prefix; only failures enter the receipt. No context gate is relaxed because a
fixture can reproduce one refusal.

## Qualified first-admission witness candidate

Source `e0ab9800`, release `b726c22e43838e36`, integrates main`0d83d045` and
qualifies the original first-failure observation above. Full local gate,
21-image isolation,488 fatal core/native-adapter cases plus actual-VMM sanitizer,
8core/14native/8VMM/4observer compiled controls, native staged bindings and strict
legacy/new receipt checks pass. Exact-stage MBR EFI refusal/checkpoint/save/
unmount/automatic return and missing-drive halt pass. The fixed log projection
is65,211/65,536 including the unchanged1,536 reserve. The image is installed with
recovery retained and no reboot armed; physical cause and timing remain pending.
The next native collector requires this row from this exact source; no rowless
legacy receipt can supply its original failed-predicate witness.

The native run `session-00065fe41fc2c614-00` now supplies that row:
`queries=3 code=7 detail=0 pages=1350 phase=1 reason=7 level=0 checked=0 value=2`.
The original entry check observes two registered address spaces and refuses
before the range walk. Wake mapping/write/engine-read counts are zero, exclusion
releases, and preparation/requester/reset do not enter. Strict decoding,
checkpoint, saved return and offline integrity checks pass; combined execution
and timing remain unaccepted. Ordinary system init precedes this diagnostic;
qualify startup ordering to establish the required sole-kernel-root context.
Do not drop root/occupancy checks or infer individual root owners from the count.

## Isolated pre-user-init startup

The held-reset image now links its own main object. Only that image, with the
complete strict18-token held-reset recipe, defers `/init --system` and reports
`init: deferred=held-reset-preinit`. Kernel USB, logging, scheduler and timer
services still initialize normally. The capture branch is terminal: it saves,
unmounts and returns to Ubuntu, or halts visibly on uncertainty/persistence
failure. It cannot fall through into an ordinary desktop without user init.
Other images and incomplete, duplicate or foreign recipes keep normal system
init and its existing startup/readiness handling. No root is discarded and the
original sole-root/count/occupancy and entry/exit query checks remain.

This lets the combined preparation/PCI/BCS8-reset/cancellation/preservation trial
establish its required CPU context instead of weakening admission. All original
100/300/500ms limits and GPU/HDA preservation checks remain. Host startup tests
include the actual helper with both image configurations; exact-stage and native
collection must additionally bind the deferred marker and absence of init
creation to this source. Successful startup isolation alone accepts no native
reset timing, GPU ownership, managed range or physical tear-free behavior.
