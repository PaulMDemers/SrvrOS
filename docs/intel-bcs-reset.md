# Broadwell BCS-only reset protocol

The [accepted request/ready/cancel handshake](intel-reset-readiness.md) has a
separate BCS-only reset successor in `kernel/diagnostics/intel_bcs_reset.c`.
Only `build/intel-bcs-reset/srvros.elf` links that core and its exact native
adapter. A separate firmware request and 15-token recipe replace the readiness
token with `srvros.intel-bcs-reset=blitter-only`. Ordinary startup and older
images cannot execute it. Native reset acceptance remains pending.

## Register and ordering evidence

Intel's [Broadwell PRM Volume 2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages 609–610, defines GDRST at `0x941c`. Bit 3 initiates only the blitter
domain soft reset. Hardware clears that bit on completion; a software clear
cannot cancel it. This is a non-posted register. The core only permits the value
8; it neither combines domains nor imports the full-reset bit. Any non-BCS bit
observed in GDRST refuses the operation. Initially set bit 3 also refuses it.

Page 165 defines `BCS_RESET_CTRL` at `0x220d0`. The request and ready bits clear
on engine reset. Consequently a successful reset must not be required to retain
the pre-reset request/ready value 3. The final explicit masked request cancellation
is still checked, with fresh clear low-half control and GDRST endpoints.
Upper write-mask bits remain observations, not active request state.

The pinned [Linux v6.12 reset implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c),
`gen8_reset_engines` and `gen6_hw_domain_reset`, prepares the selected engine,
issues the selected hardware domain twice, waits for each self-clear and then
waits 50 microseconds before cancellation. Its comments explain that register
state can remain volatile after acknowledgement. The core models these two
bounded pulses and measures at least 50 microseconds of settling. It does not
import retry-on-error, forced non-ready resets, all-engine resets or arbitrary
register restoration. The second pulse is the fixed successful sequence, never
a retry after a failed or ambiguous first pulse.

Neither the register definition nor the upstream sequence establishes an
outstanding system-memory write, prefetch or GPU-cache drain witness. A reset
acknowledgement therefore changes no completion obligation. All eight remain
open (255). CPU fences and the settling interval are not substitute witnesses.
There is no translation invalidation, command submission, PTE update, framebuffer
change, private backing transfer or native tear-free acceptance.

## Lifetime and stale observations

Entry needs the complete same-parent address-sealed 92-read census, unchanged
engine epoch, no enabled/pending/saved-wait/TLB-pending masks, parser-idle mask 3,
an awake pinned GT parent and no other live dependent. A copied result or stale
parent/epoch refuses entry. Fresh BCS raw words must match the observed empty,
disabled entry: 0/0/0/0/1/0/512/0. Reset and final post-state are deliberately
narrower than the hardware's general capabilities.

A registered one-use borrow pins the parent; blocked early close must issue no
store. The sequence is: fresh BCS/control/domain, request and ready, two BCS-only
reset/self-clear phases, measured settling, masked cancellation, unchanged BCS
and clear control/domain, parent check and verified pin return. Read, store,
guard and clock failures stop forward progress. There is no cleanup store after
an uncertain result, no forced pin return and no controller-reset fallback.

Immediately before the first possible GDRST store the owner engine epoch advances
once, including a definitely-unissued callback. It is never rolled back, and
overflow is refused before request. A new completion census records that epoch;
one observed across an epoch change cannot complete. Readiness and subsequent
reset entry reject older censuses. A reset cannot start with another live borrow.
This prevents a pre-reset empty-ring observation from silently authorizing work
after destructive engine state changes. The epoch is internal software state;
it is not a hardware ownership grant or a new field in existing version 4/5
receipts. A future native reset receipt must bind it explicitly.

The limits stay 10 ms/256 polls per acknowledgement phase and 100 ms for the
whole dependent, including verified borrow return, within the existing 300 ms GT
owner and 500 ms outer lifetime. Settling is also capped by 10 ms/256 iterations.
Late but verified clean return invalidates timing acceptance without inventing a
hardware pin. Any possibly issued request followed by an unresolved reset,
foreign state, lost guard, failed cancellation or changed post-state retains
ownership and exclusion until platform reset.

## Native admission contract

Before enabling a physical reset, validate all of the following together:

- A distinct image/request/exact recipe; ordinary/readiness images must remain
  unable to issue GDRST. Bind this Air's identity, cooperative firmware submission
  policy, fresh cache/resource/root guards and unchanged protected backing.
- A separate reserved, exact `0x9000` MMIO page with independent effective-UC/NX
  validation, prepared before wake. Existing mapping allowlists stay unchanged.
  The actual native adapter must independently admit only `0x941c`/value 8 and
  the exact BCS masked request/cancel stores.
- Same-boot pre/post other-engine and display-reader preservation, explicit
  reset-side-effect limits and protected inherited allocations. HDA/reader
  evidence from earlier separate boots cannot substitute for joint ownership.
- Raw reset/control/post-state/epoch receipts, unchanged time/count bounds, a
  strict native collector, saved prerequisite checkpoint and sufficient saved
  tail capacity. Actual compiled native MMIO/mapping negative controls are
  required; host callbacks alone cannot validate those addresses.
- Exact-stage VM no-store refusal and failed-persistence/retained-state tests,
  verified installed recovery and USB media, automatic safe return and visible
  halt on uncertainty. Volume 1%, no interactive input required.

The separate milestone of outstanding-memory completion and maintained CPU/GPU
cache/translation ownership remains necessary before changing GGTT/backing or
page-flipping. Do not treat this host model as hardware readiness.

## Historical host-only validation checkpoint

Source `e14c7bd6` integrates main `a9c1f714`. Full local host/core/profile/docs/
build checks pass, with the opt-in installed-root check explicitly omitted.
The final fixture extension passes separately. Fatal ASan/UBSan runs 144 cases;
12 compiled negative controls are detected. The core also compiles with kernel
freestanding flags. Ordinary Q35/OVMF boot and xHCI typing pass against matching
rebuilt artifacts; the kernel symbol table confirms this reset core is absent.
Initial test-wrapper/foreign-domain-assertion and small-ESP nonpasses are retained
with the final evidence. No timeout or production refusal was weakened.
See the [unit checkpoint](machines/macbookair7-2.md#controlled-reset-host-checkpoint-native-entry-still-disabled)
for retained evidence and the remaining native boundary.

## Native adapter and preservation

The native adapter prepares 13 NX/effective-UC aliases before any wake store.
Slots 0–8 keep the exact completion-audit layout; slot 9 is the separate reserved
`0x9000` page and slots 10–12 are the three read-only reader pages. Only slots
0 (wake request), 7 (BCS) and 9 (GDRST) are writable. A new cache predicate admits
only the exact GDRST page without expanding older allowlists. Only `0x941c`/8
and `0x220d0`/the two masked request values can be stored by this adapter.

The v6 wake receipt binds 13 retired aliases/cache checks and 184 pre/post
engine reads. Earlier wake versions retain their exact counts. Both censuses
contain 92 words, with post-state using a distinct `intel-bcs-post-` prefix;
its read-only scope has no reset writer. Both full passes of every engine and
context must match across reset, while the post census binds the advanced epoch.

Two same-lifetime reader snapshots retain 54 raw DWORDs for primary/cursor/sprite
controls, programmed surfaces and live addresses on all three pipes. Primary
and cursor fields bind the original captured state; every field must remain
unchanged. Two exact display-HDA config snapshots retain all ten package fields,
including command, power and capabilities. These are preservation observations,
not a maintained HDA DMA inhibit or exhaustive firmware/reader retirement.

Raw BCS rows, four request/ready endpoints, four GDRST endpoints, epoch, counts,
settling/core/owner/reader timing and preservation outcomes are saved under
`intel-bcs-`. The strict collector derives coverage, values and comparisons;
it binds the original display, both censuses, native firmware/cache receipts,
preflight checkpoint and v6 owner. A successful core under a failed outer
lifetime remains incomplete. Post-reset observations hold a second registered
one-use borrow, exercise blocked parent close and return it only after full
preservation. A late clean core return may still run preservation, without
promoting its failed timing to acceptance. Any post-reset preservation failure keeps the
parent wake/mappings/exclusion live until reset. The sticky automatic-return
gate remains mandatory; the guest shows a warning before serialized entry.
There is no rollback or forced close after an ambiguous store.

Use `make test-intel-bcs-native build/intel-bcs-reset/srvros.elf`, stage with
`--intel-bcs-reset-trial`, then follow the exact-stage VM/install/collection
workflow. Native object/mapping negative controls, image isolation, saved-tail
headroom, offline USB/recovery and durable backups must pass before declaring
hardware readiness. No physical acceptance follows from the host adapter fixture.

## Historical Air7,2 candidate readiness

Source `1000647f`, release `6a3d6afb2f2824bd`, is installed and not armed.
The [unit readiness record](machines/macbookair7-2.md#historical-native-bcs-resetpreservation-readiness)
records full local checks,144 core/48 adapter fatal sanitizer cases,23 detected
compiled controls,14-image isolation and exact-stage Q35 refusal/save/return and
missing-workspace halt. Object/staged MMIO and reader bindings pass independently
of host callbacks. The full-width log bound is65,397/65,536bytes including1,536
allowance. Prior USB data/offline media, recovery/default and check-only reboot
guards pass; volume1%, playback disabled, no input needed. Complete evidence is
backed up outside build. Native reset/preservation acceptance remains pending;
no memory/cache/backing authority or physical tear-free presentation is enabled.

The first exact VM exposed a progress message using the structured `intel-bcs-`
namespace. The progress label now uses `intel-bcs:`; an actual-message regression
covers it, and unknown structured receipts still fail. The failed guest/payload
are retained separately. The rebuilt exact source passes both VM outcomes.

## First native return: prerequisite clock refusal

Release `6a3d6afb2f2824bd` / source `1000647f` saved a complete diagnostic tail
and returned automatically, but did not enter the reset core. The92-read
pre-census found stable engine/context state in63,482us, then reported reason2
from its final clock validation. BCS request/reset attempts and writes were0;
all13 aliases retired, the GT owner closed and exclusion released. Strict
collection marks the run incomplete. USB prior-file hashes, offline filesystem
and recovery/default/consumed-one-shot checks pass.

A host reproduction shows that the existing final `end_us <= last_us` predicate
rejects two samples in the same microsecond despite earlier positive progress.
The physical receipt lacks `last_us`, so this explains a reproducible boundary
but does not prove the hardware's exact final clock ordering. Correct and test
that boundary without allowing backward time, a fully frozen observation or a
longer deadline before qualifying another candidate. The failed receipt must
remain incomplete. No reset/preservation or memory retirement acceptance follows.
See the [latest unit result](machines/macbookair7-2.md#latest-native-bcs-reset-trial-safe-prerequisite-refusal).

## Final-clock correction candidate

Source `5a73d72d` (main `f01a1ad4` integrated), release `ffd7200db10e1b02`,
corrects the final completion-clock predicate: a tie with the preceding
microsecond sample is valid only when the full observation has positive elapsed
time. Backward time, a frozen whole observation and the100ms overrun remain
refusals. No new hardware authority or receipt field follows from this fix.
The earlier native reason2 result remains incomplete.

Full local host/core/profile/docs/build checks pass, excluding the explicitly
opt-in installed-root check.28 clock,144 reset-core and48 adapter fatal sanitizer
cases and28 compiled controls pass, including restoration of the clock defect,
missing progress/monotonicity/deadline and an overly strict100ms boundary.
Fourteen-image isolation, actual native object/staged mappings/MMIO/reader
bindings and the unchanged65,397-byte full-width log bound pass. The exact-stage
MBR Q35/GRUB trial verifies no-store platform refusal, checkpoint/results,
exact boot options, automatic safe return and missing-workspace halt.

The [current unit readiness](machines/macbookair7-2.md#historical-final-completion-clock-readiness)
records installed recovery, unchanged prior files, clean offline media, volume1%
and check-only guards. The candidate is installed but unarmed. Physical BCS reset
and preservation still need their own successful receipt; all completion, cache,
translation, reader and backing obligations remain open.

## First acknowledged native reset: retained parent

Air7,2 release `ffd7200db10e1b02` / source `5a73d72d`, session
`00065fbabbb7b966-00`, passes the pre-census and records both BCS-domain pulse
acknowledgements, matching exact BCS endpoints, controls0/3/0/0, domain endpoints0
and1,931us settling. The97,618us body then fails mandatory borrow return with
`IBR_PARENT`/9 and parent `GT_ACCESS_CLOCK`/256. The last accepted owner sample
was298,483us; the failed clock value is not recorded. Exhaustion of the unchanged
300ms owner budget is an inference from that timing, not a directly logged cause.

Wake, borrow,13 mappings and exclusion remain retained. No post-reset census or
reader/config preservation runs; the sticky gate prevents automatic reboot.
A final diagnostic report and preflight checkpoint were nevertheless saved.
Strict collection marks the run incomplete. Prior files, offline filesystem,
recovery/default and consumed-one-shot checks pass after Ubuntu resumes.
The [latest unit result](machines/macbookair7-2.md#latest-bcs-reset-trial-acknowledged-reset-retained-parent)
retains the photo, raw timings and next scope: reduce guard overhead and expose
failed-clock detail without changing deadlines or allowing forced retirement.
No complete native reset/preservation or memory-ownership acceptance follows.

## Guarded requester grouping and clock failure evidence

The next isolated BCS image uses the optional requester/ACK/thread snapshot in
the [GT contract](intel-gt-access.md#optional-guarded-requester-snapshot), while
older images keep their individual-register path. Every grouped check still
performs full fresh admission before, inside and after the ordered RO loads.
All raw backing/PTE/fence validations remain; no guard result is cached across
operations. The source model measures8→3 complete audits per check, with exact
raw endpoint equivalence. This is host cost evidence, not native timing acceptance.

Wake v7 retains13 mappings and184 pre/post engine reads, and requires exactly
one compact `intel-clock` row after the BCS report and before the wake summary.
It binds the first failed sample, previous accepted time, phase and cause mask,
using the existing BCS owner origin, plus the snapshot attempt count. Derived
failure bits must agree with the GT clock reason; successful v7 collection needs
positive snapshot coverage and no clock fault. Unknown/duplicate/missing fields
remain errors. Earlier v6 reports retain their original interpretation; the
ffd7 retained-parent trial remains incomplete.100/300/500ms bounds, core pin
return, post-reset preservation and uncertainty retention remain mandatory.

## Guarded snapshot candidate readiness

Source `9ea439f6` / release `3ec42dd315377e65` is installed and unarmed. The
[unit checkpoint](machines/macbookair7-2.md#guarded-requester-snapshot-candidate-ready-for-hardware)
records full local checks,28 completion-clock/144 reset-core/53 adapter fatal
sanitizer cases plus the GT snapshot fixture,31 detected compiled controls and
14-image isolation. The source comparison preserves raw endpoints while measuring
188 versus268 full audits; this does not accept native latency. Five actual
native MMIO/mapping controls include the new exact requester snapshot addresses.
The conservative65,507-byte log projection keeps the existing65,536 limit and
1,536-byte allowance. Exact-stage MBR Q35 refusal/checkpoint/save/return and
missing-workspace halt, current media/recovery and check-only guards pass.

The new wake v7 clock receipt is mandatory only for this image version. Both
prior native nonpasses remain incomplete. Hardware must still establish full
pin return, fresh post-reset preservation and clean owner/mapping retirement
under unchanged100/300/500ms limits. No human input is needed for that automated
trial; safe outcomes save and return, uncertainty keeps the visible pre-entry
warning and retained-state reboot gate. No trial is armed by preparation.

## Guarded snapshot native return

Air7,2 source `9ea439f6` / release `3ec42dd315377e65`, session
`session-00065fbb6102d2c0-00`, saved a valid diagnostic refusal. The stable
92-word pre-census took53,979us and observed BCS enabled (mask16, control61441).
`IBR_INPUT`/1 rejected admission before a borrow, control store or reset pulse.
The version7 clock row has why0 and7 grouped checks. All13 aliases retired,
parent/exclusion released and zero state retained. Owner146,383us/outer224,902us
are shorter-path measurements, not a successful reset latency comparison.
No post-census or preservation occurred; reset acceptance remains pending.
Media/recovery/default/consumed-one-shot checks pass with1,380 files/116 sessions.
The next step must establish bounded same-lifetime inherited BCS disable/retirement
admission; do not suppress the enabled bit, widen budgets or reuse another boot's
retirement observation. The current trial is collected and no reboot is armed.

The separate [guarded shutdown qualification](intel-shutdown.md) now tests the
enabled-ring transition and fresh engine/reader preservation before any change
to reset admission. It has no reset writer and cannot accept this reset protocol.

## Fresh disabled observation within the reset borrow

The separate `--intel-bcs-shutdown-trial` / `build/intel-bcs-shutdown/srvros.elf`
image integrates the physically exercised **already-disabled** observation into
reset admission. It has a distinct firmware request, wake version9 and an exact
16-token recipe, adding `srvros.intel-shutdown=observe-disabled` to the reset
recipe. Ordinary, original reset and standalone shutdown images retain their
recipes. The enabled-ring disable writer remains outside this image; enabled or
nonzero-geometry entry refuses before any reset request.

`intel_bcs_reset_after_shutdown` creates its observation result internally while
holding the same registered reset borrow and after blocked early parent close.
The existing blitter observer receives a NULL writer. Its four fresh eight-word
snapshots must all bind the current parent's unchanged engine epoch and complete
92-word census, including exact zero geometry and parser/mode words. There is
no externally supplied shutdown receipt to copy from another lifetime. The
initial reset-side snapshot must still match, and all request/domain, two-pulse,
settling, cancellation, pin-return and post-reset engine/reader/HDA preservation
checks remain. The100ms dependent budget starts **before** observation; neither
it nor the300ms owner/500ms outer budget is restarted or extended. Any lost parent
or unresolved issued request retains ownership and prevents automatic return.

`intel-bcs-shutdown-row` records all32 DWORDs; its summary records the owner
origin, pre-reset epoch, timing, coverage and zero control attempts/stores.
Version9 counts216 pre/post/observation words and retains13 independently
qualified NX/effective-UC mappings. The only writable pages are the existing
requester, BCS request/cancel and BCS-domain GDRST pages. No ring-control writer,
new mapped page, HDA-MMIO, PTE/translation/cache writer or memory grant is added.

The new image uses strict version2 positional decimal DWORD vectors for completion,
reset, shutdown and preservation rows. Decoders validate exact field/order/width
and normalize every value into the existing named keys; version1 historical rows
keep their old meaning and cannot replace version2 in a version9 receipt. The
saved-tail cap stays64KiB with its existing allowance. Native collection must bind
the fresh observation, both censuses, raw reset endpoints, same-owner epochs,
blocked close, returned pins, inherited readers, native prerequisites and saved
checkpoint before accepting the combined trial. A VM refusal proves transport
and safe return only. The previous standalone shutdown boot never authorizes
this boot's reset, and no DMA/prefetch/cache/backing/tear-free acceptance follows.


Source `c8942c8d` / release `d74b95ab9b2f5d5c` is installed unarmed for this
combined trial. The [unit readiness record](machines/macbookair7-2.md#same-borrow-shutdownreset-candidate-ready-for-hardware)
records full/follow-up local checks,84 fatal sanitizer cases,12 compiled controls,
16-image isolation and exact-stage normal/missing-workspace VM outcomes. Saved
full-width projection is65,414/65,536bytes without a cap/allowance increase.
Media/recovery checks pass; no physical combined shutdown/reset acceptance exists.


### First combined native return

Release `d74b95ab9b2f5d5c` / source `c8942c8d`, session
`session-00065fcc53cff57d-00`, completes the same-borrow four-snapshot disabled
observation and acknowledges both fixed reset pulses, then reports `IBR_CLOCK`/2
during settling. Settling entry is98,247us into the100ms dependent budget;
the wrapper's post-failure endpoint is100,171us after core entry. The exact failed
sample is unavailable, so deadline exhaustion remains a strong timing inference.
The parent GT first-fault row is clear and cannot identify the inner-core cause.

Cancellation, borrow return and post-reset preservation do not complete. All13
aliases, wake, borrow and exclusion are retained; the normal automatic reboot is
blocked. A final diagnostic and matching preflight are saved. Strict collection
marks the run incomplete, with prior-file/offline media/recovery checks passing.
The [unit result](machines/macbookair7-2.md#latest-same-borrow-reset-trial-clock-failure-during-settling)
records the detailed timing and next scope: first-fault inner-clock evidence and
measured audit reduction under unchanged limits, never forced retirement or a
larger deadline. No reset/preservation or memory-ownership acceptance follows.

## Guarded endpoint bundles and first inner clock fault

The same isolated shutdown/reset image's successor uses wake version10. Its
optional `snapshot` callback reads BCS8, reset control and GDRST in the original
order with one fresh full provider admission; full core admission brackets the
whole read. Initial and final endpoint batches therefore use3 full admissions
instead of9, with identical raw fields, comparisons and store order. Old reset
images keep NULL callbacks and the scalar path. No snapshot is cached or reused.

Observation callbacks which perform no MMIO retain fresh pin/time checks. Their
read callbacks still perform full pre/provider/post admission around each group;
the intervening parent check remains. The compiled host scalar comparison records
215→198 full audits, all46 raw rows identical, including request/domain, engine,
reader and HDA values. The comparison binary is not a valid v10 native receipt
and cannot establish native timing or acceptance.

The core retains its first invalid sample, prior accepted sample and stage:
guard1, strict pre-return endpoint2 or clean-return endpoint3. Cause bits are
zero1, backward2, >100ms4 and strict endpoint nonprogress8. A separate mandatory
`intel-bcs-clock` v1 row binds these to the reset summary's origin and counts
endpoint-batch attempts. Later checks cannot overwrite the first fault. Guard
clock ties remain permitted, while the existing strict endpoint predicate remains.
A late verified clean return still fails timing without inventing a live pin.

Strict v10 collection derives cause bits and requires two endpoint snapshots for
core completion; v9 retains its uncaptured-clock meaning. The parent `intel-clock`
row is independent. The100ms dependent,300ms owner,500ms outer, acknowledgement
poll/phase and settling limits are unchanged. Full registered-pin checks,
pre/post preservation, uncertainty retention and all memory-ownership refusals
remain. The earlier d74 settling failure stays incomplete. No new register,
writer, mapping or enabled-ring admission follows from this optimization.
