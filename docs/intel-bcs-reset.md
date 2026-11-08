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

The [current unit readiness](machines/macbookair7-2.md#final-completion-clock-corrected-native-bcs-trial-ready)
records installed recovery, unchanged prior files, clean offline media, volume1%
and check-only guards. The candidate is installed but unarmed. Physical BCS reset
and preservation still need their own successful receipt; all completion, cache,
translation, reader and backing obligations remain open.
