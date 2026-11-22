# Original-owner disabled BCS entry and reset composition

This separate diagnostic composes the accepted already-disabled BCS observation
with the existing joint-owner reset-admission pipeline. Native acceptance is
pending. Read the [HDA/BCS observation](intel-hda-bcs-shutdown.md),
[joint lifetime](intel-hda-gpu.md) and [machine record](machines/macbookair7-2.md).

The original HDA owner inhibits Command6→2 before GT wake. A new sealed
read-only child then observes four BCS groups through the existing zero-store
blitter core, binding both the initial and terminal words to the original full
completion census and unchanged parent/epoch. It blocks early GT close and
returns its original borrow before takeover admission. The original completion
object stays live; its age and clock origin are never refreshed to buy time.

Only already-disabled zero geometry, exact idle context and complete source
agreement may advance. Enabled or nonzero entry refuses without a new store.
The existing takeover/preparation/BCS-reset/re-arm/claim/cancellation and full
preservation sequence remains responsible for all its own gates. The enabled
shutdown writer is a separate unexercised branch; this composition does not
pretend it has run or clear ring geometry to admit it.

Keep the whole child at100ms including borrow return, BCS core100ms, GT300ms,
HDA400ms and outer500ms. Existing150ms shutdown/readers limits stay unchanged
in their separate image. Failed source/epoch/registration/return retains the
original borrow and exclusion. No saved report or prior boot supplies a live
owner. No requester/PTE/Stop Ring/ring-start writer, memory grant, positive
completion assurance or physical tear-free claim is introduced.

The image, firmware request, exact recipe and receipts are distinct. Ordinary
boot and existing image contracts remain separate. Qualification must cover
actual source/census binding, zero write callbacks, all read/guard/clock/return
faults and retained static lifetimes; a copied census/owner/provider must refuse.
Native object, frame, image isolation, saved-tail and exact-stage VM checks are
required before hardware installation. Safe saved outcomes return automatically;
retained uncertainty halts visibly. The original already-disabled observation
remains accepted on its recorded source, not on this new composition.

## Image and collection contract

`build/intel-joint-admit/srvros.elf` uses wake receipt version23 and its own
`intel_joint_admit_firmware_request`. Stage with `--intel-joint-admit` and the
local `--laptop-probe-volume`; the exact22-token recipe adds
`srvros.intel-admission=disabled-bcs-v1` to the21-token joint recipe. The C and
Python parsers reject missing, duplicate, foreign or conflicting modes.
Only this recipe defers user init with `init: deferred=joint-admit-preinit;`.
Ordinary startup does not acquire this child or run this diagnostic.

Four `intel-compose-entry-row:` version1 records contain the raw eight-DWORD
groups. One `intel-compose-entry-summary:` binds those groups to the original
census times and epoch, unchanged GT store attempts, the sealed borrow and its
return, clock failure samples and zero BCS/reset stores or managed pages.
`tools/intel_entry_admission.py` joins these records with the complete existing
joint-owner graph. It rejects entry on legacy images, stale or replaced source,
reset admission before child return, and false completion. The native collector
also requires the unchanged pre-init recipe in both saved checkpoints.

The additional32 loads reuse the existing mapped BCS page. There are still14
GPU mappings and no new MMIO destination. A complete host path has619 engine
reads; the bounded collector maximum is4201. Source snapshots copy data only,
never a parent, census ownership object, provider or borrow. Failed source or
registration checks retain the original static owner and child.

`make test-intel-joint-admit` covers positive composition,64 new callback fault
prefixes, mutation of all92 original census words and1,917 faults in the
existing HDA boundary path, plus actual CLI and compiled recipe isolation.
These are model/host checks; only matching native evidence can accept the
composition on a physical machine.

## Qualified candidate, native acceptance pending

Source`0fadb55b` and release`0994cd9762acfca1` pass the full local28-image
build/USB/host/core/docs gate, fatal ASan/UBSan fault matrix, six compiled
provider/source/age/return/epoch controls and historical native receipt replay.
Object and staged checks verify eight read-only BCS DWORDs, unchanged existing
reset/re-arm code, original HDA halfword writes, retained static lifetimes and
2KiB frames. The native tail projection is62,884/65,536 including1,536reserve.

Exact-stage MBR/EFI QEMU passes strict compiled recipe and pre-init provenance,
unsupported-profile zero-borrow/store refusal, saved checkpoint/raw-byte
verification, unmount/automatic safe return and missing-workspace halt. The
candidate was banked outsidebuild before its native trial. The trial below
refused; host/VM qualification does not accept this composition. The earlier accepted disabled observation and the unexercised
enabled writer retain their separate scopes. See the machine record for the
next unattended trial and collection requirements.

## Native result: enabled entry refused

Source`0fadb55b`, release`0994cd9762acfca1`, session
`session-000660043f986e6c-00` finds stable enabled BCS in the original full
census: start`0x3eef000`, control`0xf001`, enabled mask16. The disabled-entry
child refuses as ineligible source before borrowing, reading or storing; no
reset/preparation/re-arm/claim is attempted. `completed=NO` and22 attention
items are a nonpass. HDA restores6/2/2/6, allfour inspections return, GPU14/HDA1
aliases retire and CPU exclusion releases. The files, filesystem, recovery and
consumed one-shot verify. No clock fault occurred; HDA86,707us and
outer137,288us remain within the original bounds.

The already-disabled HDA-only result remains a separate exact-source lifetime;
the difference's cause is unestablished. Do not repeat this unchanged trial or
force entry. An enabled shutdown would retain nonzero ring geometry and advance
the epoch; it cannot simply feed the old zero-geometry/reset contract or reuse
the old census. A separate explicit transition needs original-source binding,
fresh post-store observations, defined reset postconditions and retained backing/
reader ownership through restoration or irreversible takeover. Preserve existing
gates and deadlines until that route is reviewed and qualified. This result
accepts no enabled writer, reset composition, managed memory or tear-free scanout.
