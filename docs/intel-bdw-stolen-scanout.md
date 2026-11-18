# Broadwell existing-mapping scanout census

The first native tear-free target is a CPU-rendered desktop with synchronous
primary-A flips. The [primary protocol](intel-bdw-primary-flip.md) exists as a
host-qualified mechanism; its native buffer/reader/visibility provider does not.
A complete executable blitter bootstrap is a larger, separate prerequisite if
we must install new GPU translations. This census investigates a narrower path:
can the reference Air reuse two *already mapped* stolen-memory spans outside
the entire inherited framebuffer fence? A matching translation does not make
that memory available to the OS.

## Scope and evidence

The opt-in `intel-stolen-v49` workflow selects the separate
`build/intel-stolen/srvros.elf` image. Ordinary kernels contain only refusal
stubs. It performs read-only PCI/MMIO observations, with no GPU PTE, command,
plane, framebuffer, cache-control, remapping or firmware-mailbox writes. CPU
mapping operations are limited to one temporary read-only/supervisor/NX/UC
4KiB MMIO alias. It cannot allocate or transfer stolen RAM.

The local Ubuntu inventory reports locked GGC2c1, BDSM8c000001,
BGSM8b800001, TOLUD90000001 and stable STOLEN_RESERVED8ff00007:64MiB DSM,
a separate8MiB GTT allocation and a1MiB reservation at the top of DSM.
These are Ubuntu observations, not SrvrOS native acceptance. The existing
SrvrOS16-entry window starts at GPU34MiB; its contents alone do not establish
the rest of either proposed buffer. The first v47 native run, source21bd9f7c / release9fbbcd3ad8e3a69a,
refuses during initial admission (phase1/reason4), before new mappings or
metadata reads. Exclusion releases; all matching raw hashes, safe recovery and
offline integrity verify. It is explicitly reviewed/rejected. No native layout
or ownership is accepted.

That same raw cache capture records PAT2=7 (UC-minus), PAT3=0 (UC), with MTRR0
and effective0. The first adapter mistakenly required PAT2 itself to equal UC0.
Wire2 selects observed UC3 using leaf PCD|PWT, and still requires MTRR0, PAT0
and effective0 on every page. The host fixture now uses the actual PAT layout;
forcing slot2 or accepting PAT7/MTRR WB is caught. The new native retry must
establish whether any other admission check remains unsatisfied.

The generation-specific reference audit excludes two shortcuts:

- VT-d4.1 section4.3 recommends against Device-TLB invalidations with translation
  disabled. The saved Air remapper has TES clear. Linuxv6.12 also skips these
  requests in that state; its Broadwell graphics-remapping quirk includes1626.
  Do not enable remapping or assert a whole-device GTI-empty witness from a
  narrower wait, drain or invalidation result.
- Broadwell PRM2c describes FLRCTLMSG8100, but the volume13 route is System Agent
  to Command Parser. A generic R/W table field is not permission for an IA CPU
  store. The native probe adds no such writer. PCI AF FLR is a different reset
  with PCI/display restoration requirements, not a preservation shortcut.

Relevant primary references are Intel Broadwell PRM Memory Views vol5,
Display/GT Registers vol2c and Interfaces vol13; Intel document330835-003
(GGC/BDSM/BGSM/TOLUD); and Linuxv6.12 `intel_stolen.c`,
`drivers/iommu/intel/cache.c` and `iommu.c`. Local exact reference copies/hashes
and read-only host captures remain in ignored evidence storage.

## Complete-vector classification

The pure planner accepts the captured Air host/GPU/subsystem, locked PCI bounds,
a nonzero enabled BDW WOPCM reservation, legal framebuffer dimensions and the
complete old GPU fence. Proposed linear buffers include page padding,128 PTEs
of display tail and256KiB allocation alignment. The reference1440x900,
6144-byte-pitch plan is:

| Range | GPU bytes | Pages |
| --- | --- | ---: |
| Entire inherited fence | 0..34MiB | 8704 |
| First prospective buffer | 34..40MiB | 1536 |
| Second prospective buffer | 40..46MiB | 1536 |

The native adapter requires this captured geometry. Two full snapshots read
every entry high/low/high. All23,552 PTEs must be present, flag1, low32-bit,
contiguous, equal across snapshots and disjoint physically. Every physical
extent must lie after the first DSM page, below WOPCM and outside GTT storage.
The original16-entry window and full inherited framebuffer PTE vector must
also match the earlier observations. Fit, stability and nonoverlap supply only
a layout classification, never firmware, engine, DMA, cache or reader ownership.

A fresh original window/backing/cache revalidation brackets each pass. Every
DWORD read checks the observer claim, GPU/BAR/command, original kernel root,
CPU display exclusion and actual alias. Each newly mapped page is checked
against the firmware memory map and fresh full PAT/MTRR context. Leaf PAT index3 is independently checked by the
actual alias walk. The collector also decodes the original CPU cache snapshots
for all26 MMIO pages at UC3; effective UC alone cannot bypass the MTRR/PAT checks.
Only previously
admitted BAR pages are eligible. No aperture RAM is mapped. FBC_CTL43208,
EDP PSR control/status6f800/6f840 and WOPCM1082c0 are observed, not modified.
Two disabled auxiliary samples do not prove future exclusion or retirement.

Each read-only pass has its own2-second budget, checked every64 PTEs and at
completion. The reason is the full11,776-entry pass and repeated PCI/root/alias
checks, not a hardware completion delay. This does not change any existing
100ms GT maintenance, publication or transfer deadline. Failed clocks preserve
the sampled value/previous clock. Unknown alias retirement retains the alias
and exclusion; there is one cleanup attempt and no blind release/retry.

## Trial and collection

Use the [device workflow](device-workflow.md) with `--profile intel-stolen-v49`. Historical v47/wire1 and v48/wire2 receipts remain readable,
but cannot start a new trial with the wire3 image.
Its manifest freezes the new image, actual-adapter and classifier fixtures,
sanitizers, compiled weakening controls,2KiB native frames, strict transport,
source/cache/window regressions and exact-stage QEMU save/return and
missing-workspace refusal. It is a read-only profile, not inherited from the
reset/submission validation stack. No human interaction is needed.

The exact nine-token recipe includes RAM storage, this unit's volume/release,
Intel display/backing/cache/window observations, `srvros.intel-stolen=read-only`
and safe failure return. The guest saves a durable prerequisite checkpoint
before acquiring the census alias/exclusion. Saved and unmounted outcomes return
to Ubuntu; uncertain alias retirement halts. The visible preflight explains
that an unchanged screen for60seconds needs a photograph/manual restart.
The profile is silent; launch verifies host volume1%.

The current summary is wire3; fact/run rows retain version1. Wire3 adds the
first-alias `preflight` flag to wire2 admission details. It records the first
refusing operation and last requested DWORD offset, chosen PAT slot and last
completed MTRR/PAT/effective query. Later cleanup errors do not overwrite the
first refusal. Unknown type queries remain255. V49 requires wire3; historical
v48 requires wire2 and v47 requires wire1, including incomplete results. Legacy recipes
remain byte-verifiable. No source guard or existing deadline is removed.

Summary, two fact rows and six affine-run rows preserve actual read/count/hash,
first/last, the first non-affine index/value, maps/retirement/cache/alias checks
and failed-clock samples. Affine
transport is emitted after the actual C core examines every entry; the collector
reconstructs and hashes every claimed contiguous vector and binds it to the
original display/backing/cache/window. It rejects missing/duplicate/unknown rows,
changed source, partial counts, unsafe bounds and claimed write authority.
Valid incomplete receipts now pass *checkpoint identity/preservation checking*
and collect with their named refusal, rather than failing with the misleading
"Native checkpoint mismatch". They remain incomplete, cannot be diagnosed as
complete or accepted, and can only close rejected after review. Missing packets,
changed prerequisite rows and checkpoints containing late census packets refuse.
QEMU identity refusal never becomes an observation pass. Native completion can
close only as `DIAGNOSED`; it cannot accept hardware ownership or tear-free.

The first native wire2/UC3 retry (Air7,2 sourceb4d8569b, release6816e9b06874f4f3)
stopped after startup and left only boot/prerequisite logs. Its missing summary
and exit are incomplete evidence, not a named handled refusal or safe-cleanup
receipt. The negative review preserves raw logs and offline/recovery checks;
see the [unit record](machines/macbookair7-2-current.md). Next diagnostic work
must distinguish admission/source revalidation/first access. A loop deadline
checked between operations does not interrupt a primitive that never returns.

## Pending-operation progress journal

The diagnostic adapter now saves a separate `stolen-progress.log`, capped at4KiB
per update and32 updates per paused logger lifetime. The normal complete route
uses19 updates (historical wire2 used16). It never overwrites boot/session-a/exit snapshots. The caller
refuses storage through a live or uncertain diagnostic MMIO alias and brackets
held writes with the original exclusion/root identity. Normal framebuffer
exclusion remains active. There is no display bypass or GPU writer.

A16-entry RAM ring records phase, offset, pending/completed operation edges,
mapping/retirement counts and participating CPU exclusion. Safe-boundary saves
include pending profile/source/root/console/exclusion/cache checks, each source
revalidation, metadata/vector pass and classification. Mapping, cache-policy,
actual-alias and first-read details stay in RAM until the next safe boundary.
A durable row is the last saved boundary, not proof that the next call began,
returned or was the cause of a stall. Storage itself can fail or block.

Journal writes inside a census pass consume its original2-second budget; an
immediate clock check follows each such save. No start time or deadline is reset.
Failed progress saving has first-refusal code27 (`progress-save`); ordinary
readback/committed-flush and unhealthy-workspace refusal still apply.

Current v49 and historical progress-enabled v48 manifests freeze `stolen_progress_version=1` and require the fourth raw
file on a complete return. Historical frozen manifests retain their three-file
contract. The collector parses recipe-bound, bounded, consecutive progress rows
into `returned/stolen-progress.json` before checking the exit marker, so a later
missing exit still leaves useful diagnostics. It still fails collection without
that exit, all original summary/source/retirement predicates and offline checks.
The progress JSON explicitly denies completion evidence and native acceptance.

Native progress qualification (source7c89b656/release91e478bb7b14aefa) recovered
a valid journal through step9/metadata despite missing summary/exit. This places
the saved boundary after exclusion, initial cache context and source revalidation,
with no new census alias mapped at that point. Later mapping/read/cleanup remains
unknown. The run is rejected, with raw/photo/offline/recovery proof preserved.
The next diagnostic must separate first-page cache/map admission, actual alias
validation and register access; the journal alone cannot distinguish a blocked
primitive from a deliberately retained halt.

## First metadata alias preflight

Wire3/v49 addresses the native stop at metadata. Admission now invokes the
existing sole-kernel-root diagnostic with interrupts masked, before exclusion
and again before each new mapping. Being in the kernel root is insufficient
when other roots are registered; the actual leaf observer already requires the
stronger condition. Known-invalid contexts must refuse before installing a new
alias whose cleanup would then refuse the same observer. The incoming IRQ state
is restored, later leaf/root checks remain, and a context change while mapped
still retains the alias. This closes a precondition gap, not a proven diagnosis
of the photographed stop.

Wire3/v49 addresses the native stop at metadata. Before the first load from
1082c0, save the cache-policy boundary (step16), validate the resource and strict
UC3 policy, then save the pending mapping boundary (step15). Map its exact4KiB page with
the original RO/supervisor/NX/UC3 policy, validate the actual leaf/root/token,
and unmap with exact retirement checks. There is no register load in this
preflight. A retained or uncertain alias still halts without a storage write.

Only after that lifetime retires does `preflight=1` become true and the journal
save step18/first-MMIO-read with one mapping and one retirement, no live alias.
The real load remaps and repeats every original resource/cache/identity/alias
check; the retired preflight is not reused as authority. The preflight and its
journal writes consume the same original2-second pass budget, with immediate
clock checks and no timestamp reset. Normal route counts are now53 mappings,
53 retirements,107 cache checks and70,770 alias checks; all23,552 translation
checks and70,664 actual MMIO DWORD loads remain. Nineteen journal saves fit the
unchanged32-update ceiling. Legacy counters remain exact for older wires.

A step16 stop precedes the first cache-policy return; a saved step15 follows
that return but precedes the cold mapping. Neither attempts a new metadata load. A saved
step18 establishes that the preceding cold CPU-alias lifetime retired. Neither
label identifies the precise later instruction or proves a successful register
read, cleanup, GPU memory ownership or tear-free operation. Wire3 completion
requires the flag and exact counters; metadata reads without preflight refuse.

## After a native result

If the census refuses or exposes holes/aliases/reservations, do not fill or
remap them to make this route fit. Use the captured complete data to decide
whether to retain the private GPU mapping/bootstrap route instead.

If the layout fits, the next milestone is an audited, original-lifetime *borrow
provider*: explicit firmware transfer/reservation, all engine/DMA/cache/prefetch
and auxiliary-display-reader exclusion, and maintained bounds through return.
It must preserve the old fence and reject ordinary clients until restoration.
Only then may a separate adapter map the two exact CPU buffer aliases, establish
visibility and prepare pixels. The first transition from inherited X-tiling to
linear scanout needs its own ordered control/surface transaction; the current
same-control flip core does not authorize that change. Finally connect latch
checking and proven old-reader retirement before reuse, compositor scheduling,
and repeated native preservation/restore/endurance tests. No census flag closes
any of those milestones.

### Native wire3 precondition refusal

Source `87922b9f` / release `7c8aa2e4c47b3626` returned with a complete exit
snapshot but an incomplete census: first refusal5/root, phase0, no exclusion,
no new mapping or metadata read, and no retained alias. The separate progress
journal ends at root/PCI admission (step3). Return integrity/recovery pass and
the trial is rejected. This is a known-invalid admission context; the exact
sole-root subcondition is not yet identified. Instrument the original check's
value-only failure result rather than retrying a second probe or relaxing its
predicates. Earlier wire2 metadata stops remain separate incomplete evidence.
