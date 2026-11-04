# Disk installation and recovery

## PR342 integrated-main review and next bounded checkpoint

User authorizes review and merge if ready. Reviewed `1a19e2c6` against actual
main `7ef140ef`, merged as `e5088ac2`, with Ports `78fb8184`. No material blocker
or unresolved review thread. The read-only exporter preserves explicit profile/
document selection, v1 codec bounds, historical source receipts, final source
revalidation and exclusive checked output publication. Native fd/range/path/
allocation/namespace admission and deferred framed output remain unchanged.
Uncertain output publication correctly fails without overwriting or deleting a
visible bundle. It does not yet provide durable export-publication reconciliation.

Fresh combined checks pass: 70 passing installation tests and one optional
long-matrix skip, including 15 export and 19 inspection groups; profile bundle/
adapter, preferences/startup/catalog/chooser and guest-profile checks pass.
The matched full build is clean `87c5b786`. All ten proof-consumed kernel/init/
EFI/desktop/native-reader artifacts exactly match the accepted four-boot proof.
Proof tools and native sources match apart from the previously recorded trailing
blank-line trim, which produces identical native binaries. Comparing all file
payloads against the original qualified base finds only the two new bc aliases
and the already-reviewed sysabi test/legacy alias differences: 367 old files,
369 current files, 15 unchanged directory names. Current base clean/allocation/
namespace admission passes with zero write/flush attempts and unchanged bytes.
No production filesystem/profile/installer contract or consumed runtime behavior
changes; no affected VM proof needs rerun. The four-boot proof retains its actual
clean `ca90437a` attribution rather than being relabeled as a new run.

Independently streamed all seven compressed images to their original SHA-256
and byte lengths, verified 16 screenshots and 11 archived matched artifacts,
and checked all 33 proof steps: two identical exports, 16 reason-matched export
refusals, five used inspections and ten strict publication refusals. Stage/source
snapshots agree throughout; refusal output snapshots agree. No historical inode
was restored or adopted as a recovery target. The
[review receipt](assets/screenshots/installation-export-review.json), exact
checks/build metadata and streaming/comparison scripts survive at
`.local/installation-evidence/pr342-review`. Original proof receipts/screenshots
stay at their paths. Under shared disk pressure, the inactive inspection and
fsync proof images were also retained in verified gzip form with compression
manifests; no evidence bytes were discarded. Review VMs 61610/6233/85869 stay
running and untouched; no review VM, physical trial or Actions was launched.
Ready for the authorized merge; subsequent commits contain review records only.

Next proposed bounded installation milestone: **interrupted export-publication
inspection and reconciliation**. The current exporter reports failure and retains
a complete visible bundle after uncertain sync/cleanup/close, but it has no
durable export record that a later process can inspect and finish synchronizing.
Add a small export-specific publication record outside the source stage, a
read-only inspection action and explicit idempotent reconciliation of a fully
verified owned bundle. Partial, changed, unowned, linked or ambiguous artifacts
remain preserved refusals; no re-export, output replacement or source repair is
implied. This is a proposed follow-up, not work started by this merge review.

Dependencies: PR342's admitted read-only source, fixed portable payload and
checked publication boundaries; PR336's source/history distinction; the
integrated PR309/314/320/321/326 bundle, identity/preferences, pending-state and
fresh-install binding contracts; and the host identity/locking/file-directory
sync and fault-experiment patterns from PR325/330. Export records must remain
separate from installation publication receipts. Reconciliation acknowledges
the verified exported snapshot, not current source health or authentication.
Existing PR312/313/318 clean root/namespace/allocation and PR329 file-fsync
semantics suffice for restoration verification. No new kernel repair API is
required. Desktop transfer PR333 remains unmerged and separately owned; its UI
is not a prerequisite for this offline host publication slice.

Acceptance: interrupt private write, file sync, link, temporary unlink and
directory/acknowledgment boundaries in fresh processes; inspect retained state,
reconcile only exact complete owned output, and repeat without changing bundle
bytes. Refuse torn records, wrong inode/hash, unknown aliases or conflicting
destinations while preserving source images, historical receipts and evidence.
Restore a reconciled bundle into fresh B and verify after reboot. Keep physical
power loss, dirty-media salvage, in-place upgrade and authentication separate.

## Offline selected-profile export — qualified

User authorized implementation, fresh-install restoration/refusal proof and a
scoped PR, based on integrated main `a37eb7b2`. Own host export, a bounded native
read-only document reader, focused tests and proof; preserve current publication,
filesystem, profile-policy and recovery contracts. Existing review VMs and
earlier evidence stay untouched. No physical media or in-place changes.

The CLI journey was specified with optimize-ux using source inspection and an
expert walkthrough; no representative-user study is claimed. The operator has
an offline admitted used installation and explicitly chooses its UUID/home and
individual document paths. Completion is a newly published v1 portable bundle,
then exact restoration into a separately provisioned installation. Ordinary
inspection remains metadata-only and publication receipts remain historical.

| Step | Operator choice/action | Outcome or refusal |
| --- | --- | --- |
| Inspect source | Run `inspect-installed` on the offline stage | Current profile identity and structural result; pending/dirty/ambiguous state blocks |
| Select export | Supply UUID/home, repeat `--document Documents/...`, choose a new output filename | Only explicit files and portable identity/preferences; no recursive/home sweep |
| Validate/read | Export reruns admission and reads through the same locked image descriptor | Bounds, exact selection and source revalidation precede any publication |
| Publish | New file, checked file/parent sync and exclusive link | JSON success includes hashes; existing output refuses; a visible uncertain output is retained with failure |
| Restore | Use the exported bundle with fresh v2 prepare/apply and destination-local bindings | Source stays intact; verify saved content after fresh boot/reboot |

Zero `--document` options deliberately exports identity/preferences only.
Selected document contents remain opaque bytes and can contain private data;
selection authorizes those bytes. The exclusion rules prevent implicit collection
of local bindings, machine settings, caches, credentials or arbitrary application
state; they do not redact a selected document. Export refuses uncertain state
without invoking recovery or replacing output. A post-publication error means
inspect the retained output/evidence before choosing a new name; no automatic
retry, deletion or durability acknowledgment is inferred.

### Export command and accepted restoration proof

```sh
python3 tools/installation_image.py export-profile .local/installation/profile-a /absolute/new-profile.json \
  --profile-id 12345678-1234-4234-8234-123456789abc --home /fat/home/local-a \
  --document Documents/migration.txt --document Documents/binary.dat \
  --inspect-driver build/tests/installation-inspect-driver \
  --export-driver build/tests/installation-export-driver
```

Exit zero emits JSON `state=exported`, bundle/current-image/document hashes,
source-used status and inspection counters. Exit one emits `state=refused` and
a bounded reason. A post-link sync/cleanup/close error reports
`visible_uncertain=true` with the retained output path and never reports success.
An unpublished temporary cleanup failure identifies the residual artifact.
Existing outputs are never replaced; do not infer that an uncertain visible
output is absent or durably published. No publication recovery policy is changed.

Export repeats admission, matches explicit UUID/home against the current catalog,
reads through that same locked `O_RDONLY` descriptor and revalidates image,
baseline, records and driver before output publication. The stable plan lock
remains held through publication. Offline cooperative ownership is required;
these checks do not isolate a hostile arbitrary writer. The separate native
reader validates clean admission/allocation/namespace targets, bounds and regular
file parents/types, retains selected content within its budget, and emits framed
bytes only after successful unmount with no attempted writes/flushes. Inspection
keeps its existing metadata-only protocol and output budget.

The v1 codec remains normative: up to 64 explicit files, 1 MiB each, 8 MiB total
decoded documents and 12 MiB encoded bundle; ASCII components, case and parent
collisions, canonical hashes/base64 and typed portable settings all validate.
The reader supports the codec's 160-byte relative document path and up to 256
bytes after adding a 95-byte home. Fresh destination provisioning retains its
existing stricter expanded-path/save-temporary headroom checks; export does not
bypass destination admission or rename data. Older profiles without display
identity use the validated home-leaf fallback; a fallback that fails portable
identity rules refuses. Used v2 still requires retained `profile.exfat`; used v1
requires its exact original data source. Dirty or pending sources are not salvaged.

Clean `ca90437a` includes main `f0c88a6d`/PR337 and Ports `747cca28`.
The matched full build and combined checks pass: 70 passing installation tests,
one optional long-matrix entry skipped, including 15 export and 19 inspection
groups. Profile bundle/adapter, preferences/startup/catalog/chooser and guest
profile checks pass. Real 1 MiB selected-file and 8 MiB aggregate cases pass;
9 MiB refuses natively before any stdout. Corrupt/oversized frames, source/driver
change, fd/range/path boundaries and write/short-write/file-sync/link/unlink/
directory-sync/close failures refuse. A bring-up fixture-prefix failure and an
early firmware-path rejection are retained separately and excluded from acceptance.

The accepted proof has four sole-installed-disk EFI lifetimes: A edit/save and
reboot/reopen, then fresh B boot/reboot from the actual exported bundle. Both
direct exports are byte-identical. Exact text/binary bytes, portable UUID and all
six preferences match. B uses new GPT disk/partition IDs, home `local-b`, alias
`local_b`, UID/GID 2050/2051 and its own machine preferences; A retains `local-a`,
`local_a`, 1001/1001. B's preboot readback confirms only selected documents were
imported, excluding A's generated environment/cwd documents. All boot readbacks
check local bindings, machine settings, environment/cwd, application hashes,
launch order, protected regions and clean allocation. Five used inspections and
ten strict publication inspection/reconciliation refusals preserve old receipts.

Sixteen export refusals match their intended reason and preserve whole-stage,
source and output snapshots: wrong UUID/home, missing/directory/traversal/
duplicate/case-colliding documents, existing/linked/in-stage output, busy image,
protected boot padding, dirty exFAT, invalid portable settings, pending marker and
torn sibling journal. The harness alone restores its deliberate mutations and
verifies the original used-image hash. No inspector/exporter repair occurs.

The [portable receipt](assets/screenshots/installation-export-manifest.json) and
[restored document](assets/screenshots/installation-export-restored.png) record
source/build/tool hashes and the visually checked B reboot. All 67 raw proof
files, images, logs and matched artifacts were copied and SHA-256 verified at
`.local/installation-evidence/export-proof-r29pmoja`, outside build cleanup;
qualification used original target paths/inodes under `build/review/installation-export/proof-r29pmoja`.
After publication, seven inactive image pairs were preserved in verified gzip
form to release shared disk headroom. Receipts and screenshot paths stay in
place; `compressed-images.json` in the proof directory points to the archive's
full mapping, original SHA-256/size and gzip SHA-256. Every stream was decompressed
and checked before both duplicate raw files were removed. Original inode values
are now historical evidence; a decompressed file is not an adopted publication
or recovery target. Matched build artifacts remain uncompressed in the archive.
All task VMs stopped; review VMs 61610/6233/85869 remain running and untouched.
To preserve reserve space, superseded IO proofs `oyq2ene6` and `j8xbfmgc` now keep
their six large images in verified gzip form under their existing evidence
archives, with `compressed-images.json` recording byte hashes, sizes, round-trip
verification and historical inode identities. These are historical evidence,
never adopted recovery targets; the later fsync, inspection and current export
proof targets remain intact.

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py -D_DARWIN_C_SOURCE -Wno-array-bounds' test-installed-root test-profile-desktop test-profile-chooser test-guest-profiles
sh tools/macos/build.sh -j2 all
python3 tools/installation_export_smoke.py --ovmf /opt/homebrew/share/qemu/edk2-x86_64-code.fd --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

The proof is bounded to generated clean offline images. It grants no dirty-media
salvage, repair, in-place upgrade, authentication, live health, power-loss or
physical-media qualification. Open desktop-transfer PR333 remains independently
owned; no unmerged profile/UI behavior is consumed. Publish a scoped draft for
independent review; merge remains separate.

Final publication refresh integrates main `819956be` (PR338 managed-salvage
qualification and PR339/340 app-closure investigation). Fresh combined host checks
and matched build pass. No production kernel/filesystem/profile/installer contract
changes. All ten consumed kernel/init/EFI/desktop/native-driver artifacts, proof
tools and native source hashes remain identical. A complete comparison of 367
file payloads and 15 directory names differs only in the explicit `sysabi`
managed-salvage test executable and its legacy alias (`bin/sysabi`, `bin-sysabi`);
every other payload and directory name matches. The new base passes native
read-only clean/allocation/namespace admission with zero writes/flushes. No
affected four-boot behavior requires rerun; evidence retains clean `ca90437a`
attribution and records this refresh separately. Refreshed build metadata/logs
and comparison survive beside the accepted raw proof. PR338 is consumed;
desktop transfer PR333 stays separate.
The later shared-header EOF trim removes only an extra blank line; it changes
no preprocessed tokens and claims no new build or VM proof. Combined runtime
verification remains separate from this held draft and awaits dependency merge
identities; no new image set is allocated during that wait.

## PR336 integrated-main review and next bounded checkpoint

User authorized review and merge if ready. Reviewed `b3c6a694` against fetched
main `62b0708e`; no material blocker or unresolved review thread. The separate
inspector retains strict publication/reconciliation refusal after guest saves,
uses only an inherited read-only image descriptor in the actual kernel exFAT
reader, and checks historical protected bytes and current bounded profile
metadata without repair or receipt replacement. Profile preference grammar,
startup size limit, optional legacy identity, pending-import/scratch rules and
remembered-choice CRC were checked against the integrated guest consumers.

Fresh installation checks pass (55 passing, one optional long-matrix entry
skipped), including all 19 inspection groups. Profile bundle/adapter, desktop
preferences/startup/catalog/chooser and guest-profile checks pass. Documentation
checks pass. All ten accepted artifacts, nine proof tools, five native source
files and three matched build outputs have the accepted hashes. The retained
used stage matches its final accepted snapshot and receipt. Two fresh CLI used
inspections pass with zero native writes/flushes; strict publication inspection
and reconciliation still refuse it. All four calls preserve the whole stage,
original source inputs and historical receipt.

Main and runtime inputs are unchanged, so no affected VM proof requires rerun.
The original two-boot, six-positive, 23-mutation-plus-busy and four-strict-refusal
proof remains attributed to `ff974bdf`. The
[review receipt](assets/screenshots/installation-used-review.json) records the
fresh checks and equivalence. Logs and verification script survive outside build
cleanup at `.local/installation-evidence/pr336-review`; the original full proof
and matched artifacts remain separately archived. Review VMs 61610/6233/85869
remain running and untouched; no new VM, physical trial or Actions was needed.
Ready for the authorized merge; follow-up commits contain only review evidence
and continuity documentation.

Next proposed bounded installation milestone: **read-only selected-profile
export from a used installed image**. PR321's migration proof reads persisted
bytes into a host fixture before exporting. Supply an actual offline export
command that binds an admitted inspected image and explicit profile UUID/home,
reads only individually selected `Documents/...` files and portable identity/
preferences, and creates a new portable bundle. Preserve the used image and its
historical receipts; restore through the existing fresh-install plan rather
than changing the source installation. This is proposed work only.

Dependencies and boundaries:

- PR336's stage/target identity, protected-region proof, cooperative offline
  locks and refusal rules. Revalidate the same image through document reads;
  a stale prior inspection result is insufficient. Add a bounded `O_RDONLY`
  selected-document reader: the current native bridge intentionally emits only
  metadata and cannot export document contents.
- Integrated PR309/314/321 profile bundle/provisioning contracts, PR320 import
  boundaries and PR326 identity/local-choice semantics. Reuse the normative v1
  codec/path rules and limits (64 explicitly selected files, 1 MiB each, 8 MiB
  decoded aggregate, 12 MiB bundle); preserve portable UUID and validate any
  legacy identity fallback explicitly. Local alias/UID/GID, machine settings,
  caches, credentials and arbitrary application state are excluded.
- Integrated PR312/313/318 clean root/namespace/allocation admission and PR329
  file-fsync semantics; keep source inspection read-only and publish output
  exclusively with checked file/directory sync. Dirty, pending, torn, ambiguous,
  changed or unsupported source/output states refuse without replacement.
- Use the existing matched-build/provenance and v2 fresh-install adapters for
  restoration proof. No new kernel repair/salvage API is required. Open PR333
  owns desktop reviewed transfer and depends on SrvrApps #6; a host offline CLI
  need not wait for or duplicate that UI. Open PR338 qualifies managed workspace
  salvage without production contract changes; it is not an export dependency.

Acceptance: edit/save/reboot installation A, export the actual stopped image
through the new reader without an intermediate host profile fixture, then
provision and reboot fresh B with different disk/home/local bindings. Verify
selected text/binary bytes, portable UUID and all six preferences; prove
unselected files and local/machine state are excluded, source/receipt hashes
unchanged, and changed/unsafe/oversized/pending/conflicting cases refuse without
publishing success. No dirty-media salvage, in-place upgrade, authentication or
physical-media claim belongs to this slice.

## Read-only used-installation inspection — qualified

User authorized implementation, used-image/refusal proof and a scoped PR.
Branch `device/apple-silicon/installed-state-inspection` reuses isolated
`SrvrOS-installation-io`, main `5634cd9d` (PR330, hardware-evidence-only
PR331, additive filesystem proof PR332 and SQLite integration PR306). Current filesystem/profile contracts include PR312/313/318 root and
allocation admission, PR320/321 import and binding, PR326 identity/selection,
and PR329's file-scoped fsync versus global sync/reboot boundaries. Open PR317
and browser/hardware work were inspected; no installation/profile overlap is
consumed. Own a separate host inspector, bounded read-only native metadata
reader, CLI/tests and proof. No kernel, provisioning or profile-policy edits.

| Offline stage | Inspection action/result |
| --- | --- |
| Original sealed publication, current data unchanged | Validate immutable boot/root identity, current filesystem and profile; report consistency |
| Same installed image after legitimate saves | Allow mutable data changes; report used state and current metadata while preserving the original receipt as history |
| Wrong boot/GPT/root identity, allocation inconsistency or invalid profile | Refuse with bounded diagnostic and preserve all bytes |
| Pending/torn import or unknown transaction/selection scratch | Report blocked/refused; never run recovery, remove markers or choose a profile |

The new `inspect-installed` path is separate from strict publication `inspect`
and `reconcile`; guest-modified output remains ineligible for publication recovery.
It uses retained plan/journal/ready/bootstrap records. Used v2 images also need
the unchanged retained `profile.exfat`; other original provisioning sources may
be absent. Used legacy v1 images need the exact original exFAT source as their
data baseline, otherwise inspection refuses with a baseline diagnostic. The
current protected image bytes are combined with that verified original partition
and checked against the original whole-image hash; all GPT/ESP/padding bytes
remain bound while current user data may differ. The original private directory and candidate inode still
bind the inspected target. A native reader receives only an inherited regular
`O_RDONLY` image descriptor and a checked partition range, runs actual exFAT clean
admission/allocation checks and returns bounded metadata with zero attempted
writes/flushes. It checks the backing directories required by the existing
namespace bindings. The existing `O_RDWR` provisioning adapter is not reused.
Profile identity display records remain optional for older v2 installations,
matching the guest fallback; present records must validate. Portable settings
use the guest's 512-byte limit and remembered selections require its exact CRC.

Acceptance includes a stopped used image after edit/save/reboot, exact identity
and data readback, structural/profile refusals, busy/unsafe input refusal and
whole-stage/source/receipt preservation. Matching on-disk structure is not a new
publication acknowledgment, authentication, live health, repair or physical-media
acceptance. Cooperative offline ownership is required. Preserve review VMs
61610, 6233 and 85869, primary checkout and earlier evidence. One build/harness
at a time; no physical disks, host reboot, Actions or merge requested. Status:
implementation and proof complete; publish a scoped draft for review.

### Inspection contract and accepted proof

```sh
python3 tools/installation_image.py inspect-installed .local/installation/profile-a --driver build/tests/installation-inspect-driver
```

Exit zero and `state=installed-consistent` mean the stated offline structural
checks passed. Exit one emits a JSON refusal reason. The result reports whether
the image differs from its original publication (`used`), current image hash,
historical publication/data baseline, GPT identities, filesystem admission and
allocation counts, namespace backing targets, profiles and remembered choice.
It does not write a replacement receipt, choose a profile or certify live boot
readiness. Keep the image offline; nonblocking plan/image/baseline locks serialize
cooperating tools, not arbitrary writers. Copied/replaced stage/candidate inodes,
partial aliases, unknown entries, unsafe links and incomplete records refuse.

The original source files are not used as a release dependency for v2 inspection:
retained records and the canonical bootstrap prove boot payloads, while the
unchanged retained `profile.exfat` allows streaming recombination of current
protected regions with original data. Pristine images can prove the original
whole hash directly. Used v1 images instead require their exact original exFAT
source inode/device/size/hash; unavailable baselines refuse without guessing.
Existing publication `inspect` and `reconcile` retain their original stricter
source/output checks and continue to refuse guest-used images.

The native bridge accepts only an inherited `O_RDONLY`, single-link regular
image descriptor and a checked aligned range. It uses the actual kernel clean
exFAT mount/allocation checker, verifies `/fat/bin`, `/fat/var`, `/fat/home`
backing directories (`/usr` maps to `/fat`), and fails if any write/flush is
attempted. It does not reuse the provisioning driver. Limits are 2 GiB exFAT in
the frontend; 128 MiB live native heap, 2 GiB aggregate reads, one million native
read calls, 4096 emitted metadata entries, 159-byte paths and 1 MiB output.
Large/unsupported inputs may be refused rather than partially inspected.

Profile checks preserve current guest semantics: UUID and planned local binding,
required directories, optional legacy identity display record, literal portable
preference keys/canonical unsigned values within the guest's 512-byte startup
limit, profile ambiguity and the remembered-choice CRC. Present malformed or
mismatched identity records refuse. Any import journal, pending marker or unknown
transaction/selection scratch blocks inspection; no recovery authority is inferred.
Ordinary document/settings changes remain allowed.

Clean candidate `ff974bdf` includes main `5634cd9d` and Ports `525f4229`.
The matched full build and combined host checks pass: 55 passing tests and one
long-matrix entry skipped by default, including 19 new inspection groups;
profile/session/preferences/chooser/catalog and guest-profile checks also pass.
New coverage includes API/CLI snapshot equality, v1/v2 historical baselines,
missing original v2 inputs, older v2 identity fallback, exact 512/513-byte and
guest JSON grammar boundaries, locks, read-only fd/range enforcement, structural,
metadata and source-identity refusals. A stale-helper development run is retained
as failed bring-up evidence and excluded from acceptance.

The real installed-image proof passes two VM lifetimes (edit/save and fresh
reboot/reopen). Root selection precedes namespace activation and explicit profile
launch; text/binary documents, six portable settings, identity/local binding,
machine settings, environment/cwd, installed apps and protected regions match.
Six positive inspections include pristine, repeated post-boot used states and the
restored final image. All show zero native write/flush attempts and preserve the
whole stage and source hashes. Four old publication inspection/reconciliation
calls refuse used output without changing its original receipt.

Twenty-three in-place mutations plus a busy-image-lock case refuse with the
expected diagnostic and identical before/after snapshots: kernel/config payload,
ESP padding, GPT root/label, dirty/backup boot regions, allocation leak, marker/
identity/local/preferences corruption, pending/journal/torn/scratch/selection
state, torn receipt, unknown stage entry and missing/tampered historical baseline.
The allocation-leak refusal occurs in clean admission, which runs the allocation
check before the later explicit checker. Known fixture mutations are restored
only by the proof harness after recording refusal; the inspector performs none.
The final used image inspects successfully again.

The [portable receipt](assets/screenshots/installation-used-manifest.json)
contains source/build/reader hashes, baseline/protected-region results, expected
refusal reasons, snapshot hashes and both boot readbacks. The
[reopened document](assets/screenshots/installation-used-reopened.png) was visually
inspected. Raw images, patch records, commands, logs and matched artifacts were
byte-verified outside build cleanup at
`.local/installation-evidence/used-proof-4k419y9m`; originals remain at their
recorded paths/inodes. All task VMs stopped; review PIDs 61610/6233/85869 and the
primary checkout remain untouched. Open reviewed-transfer PR333 was inspected:
it reuses the existing import journal; this inspector never invokes its commands
or takes import/recovery authority, and consumes no unmerged dependency.

Final publication refresh integrates main `62b0708e` (PR335): its host-only
Haswell path audit, test-target addition and hardware documentation change no
installation, filesystem, profile or runtime source. Fresh combined host checks
pass; all proof tool/native-source and matched artifact hashes remain identical.
The accepted VM proof stays attributed to clean `ff974bdf`, with this unchanged
runtime equivalence recorded separately rather than relabeling the evidence.

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py -D_DARWIN_C_SOURCE -Wno-array-bounds' test-installed-root test-profile-desktop test-profile-chooser test-guest-profiles
sh tools/macos/build.sh -j2 all
python3 tools/installation_inspection_smoke.py --ovmf /opt/homebrew/share/qemu/edk2-x86_64-code.fd --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

No physical media, power loss, authentication, arbitrary writer isolation,
automatic repair or lossless recovery is qualified. Inspection reports the
supported on-disk structure, not current application authenticity or a successful
global storage flush. Publication/recovery formats and policy remain unchanged.

## PR330 merge review and next bounded checkpoint

Final refresh: main advanced to `10890eed` (PR329) during merge preparation.
Integrated it as clean candidate `740869f0`. Its file-fsync acknowledgment now
uses description writeback, retaining the backing-volume transaction barriers;
global sync/reboot barriers remain separate. Fresh file-fsync regression and
installation/profile/chooser/guest checks pass, as does the matched full build.
Because guest save persistence exercises the changed syscall, the complete real
image seven-error/two-boot proof was rerun and passes at
`build/review/installation-io/proof-par6_sd9`. All nine recorded host-matrix
component hashes remain identical, preserving its 436-case acceptance without
repeating unaffected cases. Both boots preserve identity metadata, exact saved
data/settings and the historical receipt on used-image refusal. Raw proof and
matched artifacts are byte-verified at
`.local/installation-evidence/pr330-fsync-review`; the review receipt below
records this final refresh. Review VMs remain live and untouched. This supersedes
the earlier unchanged-main equivalence-only runtime assessment below.

Initial review, authorized to merge if ready: main was `c90f3522`;
candidate `5d0fa39e` already integrates it. Independent implementation review
found no material blocker in the checked-write, reconciliation-barrier or
bounded fault-proof behavior. No submitted correction or unresolved review
thread was present. The CLI journey retains read-only diagnosis before explicit
reconciliation and never treats matching visible bytes as a new sync acknowledgment.

Fresh host checks pass: 36 installation tests plus the default-skipped long
matrix entry, profile/session/preferences, chooser/catalog and guest-profile
checks. The matched full build reproduces every recorded runtime artifact and
all matrix/smoke tool hashes. Both raw receipt hashes match, and the current
post-guest stage still matches its complete final snapshot. The 436-case matrix,
seven refusal controls, two negative controls and current-main seven-error/
two-boot proof remain applicable by exact equivalence; no affected proof was
invalidated and no new VM was needed. The
[review receipt](assets/screenshots/installation-io-review.json) records fresh
checks and equivalence; logs/build metadata survive outside cleanup at
`.local/installation-evidence/pr330-review`. Review VMs 61610, 6233 and 85869
remain live and untouched. Ready for the authorized merge.

Next proposed bounded installation milestone: **read-only post-boot installation
inspection**. A legitimate save changes the image hash and correctly makes
publication recovery refuse; operators still need to inspect that used image
without relabeling it as the original unbooted candidate. Add a separate offline
inspection path for disposable installed images, reporting stable disk/root and
boot-payload identity, current filesystem admission/allocation state and planned
profile metadata while allowing ordinary document/preferences changes. Keep
the original ready receipt historical and `reconcile`'s used-image refusal intact.

Dependencies: PR325/330 publication receipts and identity boundaries; the existing
GPT/ESP/root-selection and namespace contracts from PR312/313 and PR318's
filesystem acceptance/allocation checks; PR321's v2 profile binding; PR320's
pending-import/unknown-scratch rules; and PR326's identity/local-choice metadata
and recovery preflight. The existing allocation-check mode opens read-only;
profile metadata needs a bounded read-only reader/adapter, because the provisioning
adapter opens `O_RDWR` and must not be used for this inspection path. Reuse the
current matched source/driver provenance checks. No unmerged salvage PR or new
kernel repair API is required.

Acceptance: edit/save/reboot, then inspect the stopped used image successfully
without changing any bytes; distinguish boot/GPT/root/profile mismatch, invalid
allocation state and pending/torn/unknown profile state with preserved refusal
evidence. Report on-disk consistency, not authentication, live health, repair,
power-cut durability or physical-media acceptance. This is a proposed next slice;
implementation is not started by the merge review.

## Host publication I/O failures — qualified

User authorized deterministic write/sync/link/unlink qualification and a scoped
PR. Branch `device/apple-silicon/install-publication-io-qualification` uses the isolated
installation clone and integrated main `c90f3522` (PR324/325/326/327/328). Own the host publication
tool, fault fixture/tests and installation evidence. No kernel, filesystem,
guest profile or application policy changes. Review VMs 61610, 6233 and 85869
remain live and untouched; one build/harness at a time; no physical disk, reboot
or Actions. Status: qualification complete; publish a scoped draft PR.

Current dependencies are PR325's journal/identity/locking, PR321's version-2
plans, PR320's pending-import/unknown-state refusal and PR322/323's managed
writeback/salvage limits. PR326 adds profile selection,
recovery status and offline display-name metadata, without changing the host
publication contract. Its offline `identity.json` addition is integrated before
the accepted fault matrix and production-image proof. PR327's
descriptor-preserving salvage helper was inspected and merged into main during
this work; its caller-owned descriptor and explicit
success/error boundaries neither grant host repair authority nor gate this work.
PR328's additive installed-root salvage consumer and default-preserving harness
arguments are also integrated. They do not overlap this host publication path.

Separate storage-guard work appeared in the earlier installation checkout.
Its edits remain intact there. This milestone moved only its own changes into
`SrvrOS-installation-io`, a separate clone with private submodules/build outputs;
the app's managed-worktree tool could not select the nested source repository.
Existing build seeds were cloned with copy-on-write, then rebuilt locally.

Qualify real private-stage host I/O through deterministic adapters: errors
before and after sync/link/unlink effects and partial metadata/image writes.
The operation must not acknowledge success after an injected failure. Fresh
read-only inspection must preserve the stage; explicit reconciliation must
revalidate the complete existing record/candidate, re-establish file/directory
sync barriers, and publish identical image bytes or refuse without mutation.
Already visible ready bytes do not waive a failed durability barrier. Unknown
and preexisting state, original sources and guest import markers stay preserved.
Cover ordinary-v1/profile-v2 plans and reconciliation itself, with explicit
adapter limits, negative controls and raw evidence outside build cleanup.
This is deterministic host error handling, not a power-cut/media-durability
guarantee or automatic retry/repair.

Metadata records are serialized canonically into one checked write; bootstrap
and configuration byte counts are checked too. The bounded matrix samples the
first write per path, with all top-level publisher writes covered and sparse
image/provisioning-adapter sampling limits explicit. The separate production
proof uses the actual single-write image builder and real profile driver.

### Accepted I/O-failure evidence

The bounded matrix at clean `3eae3223` passes **436** selected faults across
11 baseline scenarios: legacy-v1, identity-v1 and profile-v2 apply, plus four
reconciliation states for identity-v1 and profile-v2. Every selected fault fires
and the operation reports failure. There are 64 write, 36 explicit flush,
278 file/directory fsync, 26 link and 32 unlink cases. Modes include 208 errors
before effects, 186 after effects, 22 prefix-write errors and 20 short returns.
All 300 eligible cases finish explicit reconciliation with identical candidate
bytes; 136 unverified/torn cases refuse without stage mutation. Source type,
permissions, device/inode, link count, size and bytes are checked across both the
failed operation and fresh-process inspection/reconciliation (access times are
excluded). Repeated recovery preserves bytes and namespace, while reissuing syncs.

Seven additional controls preserve/refuse torn journal/ready, unknown entries,
an unknown symlink, an outside image hard link, changed source and changed image.
Two in-memory mutants of actual production reconciliation are rejected: the
old already-ready shortcut and swallowing an existing-file fsync error.
The sparse GPT/FAT builder samples its first MBR write, and the profile adapter
uses a subprocess stub; later sparse writes and the stub's internal I/O are not
claimed as exhaustively injected. Disposable matrix stages are removed only by
the test harness after snapshots and recovery checks, not by product recovery.

Current main `c90f3522` was then integrated, including the Ports `ca749517` pin,
hardware diagnostics and strict host-build fixes. Every recorded Python matrix
component hash still matches exactly; the receipt records that equivalence,
retaining the original matrix source attribution. Fresh installation/profile/
chooser/guest checks pass: the default discovery run has 36 passing tests and one
opt-in matrix entry skipped because the complete matrix ran separately.
Both native host drivers were rebuilt with captured commands/source/output
hashes, and EFI bytes match the pinned prebuilt Limine Git blob.

Clean current candidate `b6ffb556` passes the matched full build and the separate
production-image proof. Six faulted API processes exercise a failed publication
journal fsync, post-effect image link/unlink, failed ready-file fsync and
post-effect ready link/unlink. Fresh CLI inspections diagnose each state; final
reconciliation and its repeat preserve the same candidate. A seventh real-image
write fault leaves a partial legacy image which inspection/reconciliation preserve
and refuse. The actual image builder and profile driver are used in this proof.

The reconciled image then passes two VM lifetimes: edit/save and fresh
reboot/reopen. Root identity precedes namespace activation and explicit profile
launch. Exact text/binary documents, six portable settings, UUID/local binding,
machine preferences, environment/cwd, installed apps and `identity.json` match.
Guest/offline allocation checks and protected GPT/ESP regions pass. After each
boot, inspection and reconciliation refuse the changed image without altering
any bytes or the original ready receipt. All task VMs stopped; review PIDs
61610, 6233 and 85869 remain live and untouched.

The [portable receipt](assets/screenshots/installation-io-manifest.json) records
coverage, negative controls, source equivalence, production fault snapshots and
both boot readbacks. The [reopened document](assets/screenshots/installation-io-reopened.png)
was visually inspected. Full raw receipts, stages, logs and matched artifacts
are byte-verified outside build cleanup at
`.local/installation-evidence/io-proof-j8xbfmgc`. Original recovery targets stay
at their recorded paths/inodes; backups are audit copies. The earlier passing
runtime and its artifacts are retained separately as `io-proof-oyq2ene6-original`.
Interrupted development matrices and the resolved temporary-path bring-up failure
remain recorded and are excluded from accepted counts.

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py -D_DARWIN_C_SOURCE -Wno-array-bounds' test-installed-root test-profile-desktop test-profile-chooser test-guest-profiles
sh tools/macos/build.sh -j2 all
python3 tools/installation_io_faults.py run --output build/review/installation-io-faults.json
python3 tools/installation_io_smoke.py --ovmf /opt/homebrew/share/qemu/edk2-x86_64-code.fd --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

An error may leave complete visible bytes, including a ready receipt. `inspect`
is still a read-only diagnosis, never a new durability acknowledgment. Explicit
successful `reconcile` must revalidate and complete all required host syncs;
another sync error remains a failure. No automatic retry, unknown-state cleanup,
profile import-marker clearing or managed workspace repair is introduced.
Open PR329's guest file-fsync isolation/dirty-source proof was inspected and is
independent of this host path; it is not imported as a dependency. No physical
media, power cut, kernel/device fault, profile-driver subprocess I/O or arbitrary
hostile-writer qualification is claimed. Journal integrity is not authentication.

## PR325 integrated-main review and next checkpoint

User authorized review and merge if ready. Candidate `8b4619b6` integrates main
`4f5edb0e`, including PR323's retained-reader salvage qualification. Main added
only host fixtures and documentation; no kernel, installed userspace, profile
policy, installation tool or exercised VM harness changed from the accepted
`291f469e` proof. Independent implementation review found no material blocker
within the private, offline, cooperative-writer scope. No submitted correction
or unresolved review thread was present.

Fresh 35 installation/provenance tests, installed-root admission checks,
profile/session/preferences checks, 13 bundle groups and seven adapter groups
pass. A sequential matched full build reproduces every kernel, init, initramfs,
exFAT, EFI and host-driver hash used by the original proof; its six recorded
recovery/profile tool hashes also match. The nine CLI interruptions, sixteen
refusals, two interrupted reconciliations and two persistence boots therefore
remain valid by exact source/artifact equivalence; no new VM was needed.
The [review receipt](assets/screenshots/installation-recovery-review.json)
records the comparison and fresh check logs, preserved outside build cleanup at
`.local/installation-evidence/pr325-review`. Review VMs 61610, 6233 and 85869
remain live and untouched. The premature overlapping build invocation is retained
as history; only the subsequent sequential build is accepted for this review.
Status: ready for the authorized merge, with process-interruption limits intact.

Next proposed bounded installation milestone: **host publication I/O-failure
qualification**. Deterministically fail metadata/image writes, file and directory
syncs, publication links and alias removal on disposable stages. Require a
non-success result on each failure, exact preservation of source inputs and
unknown/preexisting state, conservative read-only diagnosis after restart, and
explicit reconciliation only when the existing verified record and candidate
still satisfy the complete contract. Record uncertain artifacts without cleanup
or automatic retry. Qualify both ordinary-v1 and profile-v2 plans, including a
reconciled-image boot/persistence control if runtime output changes.

Dependencies: PR325's verified journal/identity/locking rules and PR321's v2
plans; the existing GPT/ESP/root-selection and filesystem allocation checks;
PR320's pending-import and unknown-scratch refusal rules; and PR322/323's error
and lifetime boundaries. Their managed dirty-data salvage is a separate path:
publication must neither clear failure latches nor replay provisioning after an
ambiguous error. The proposed userspace salvage helper is not a prerequisite.
This next slice is deterministic host I/O-error qualification, not physical
media, power-cut durability, authentication, in-place upgrade or automatic
repair. It is a proposal, not implementation begun by this review.

## Interrupted installation publication — completed proof

User authorized inspect/reconcile implementation, interruption/refusal proof and
a scoped PR. Branch `device/apple-silicon/install-publication-recovery` uses the
isolated installation clone, main `0cba30a1` after PR322 landed. Consume PR321's
version-2 profile plans, PR320's pending-import/unknown-scratch preservation rules,
and existing GPT/boot/hash validation. PR322 `312f4f15` was inspected before its
merge: six host fixture/tool/docs files, no production change. It qualifies
managed workspace failure retention, not host staging recovery or guest faults.

Own installation publication API/CLI, focused tests, new process-interruption
proof and this record. No kernel/filesystem/profile-policy implementation or
guest import journal changes. Before publishing image aliases, persist an
exclusive verified-publication record binding plan/source/target identity and
the complete output. Inspection is read-only. Explicit reconciliation may link
only that fully verified output and its canonical ready receipt; it must not
re-provision data, rebuild GPT or discard unknown state.

| Observed state | Operator action / completion |
| --- | --- |
| Prepared | Normal apply is available; no output has been verified |
| Interrupted without a complete verification record | Preserve evidence and prepare a fresh target; reconciliation refuses |
| Verified bytes, publication incomplete | Inspect identities/hashes, then explicitly reconcile publication |
| Ready and unchanged | Inspection diagnoses matching visible bytes; explicit successful reconciliation re-establishes sync barriers before recovery completion |
| Changed, torn, unknown or linked outside the stage | Preserve all bytes; refuse publication/reconciliation |

Use one cooperative publisher at a time. Reconciliation must revalidate while
holding admission before any mutation. Named interruption checkpoints cover
successful host write/flush/link boundaries; this is process-restart proof,
not media-error recovery, power-loss atomicity or PR322's managed-retirement
contract. A guest-modified image is outside unbooted publication recovery and
must not be relabeled as the original verified output.

Acceptance: fresh CLI processes interrupted at provisioning, assembly and
publication boundaries; early refusals preserve state, later verified stages
reconcile, interrupted reconciliation can finish, and repeated reconciliation
does not mutate a ready stage. Modified inputs/output, torn metadata, symlinks,
outside hard links and unknown files refuse without cleanup. A reconciled
profile image must boot, edit/save and reboot with exact profile persistence.
Exact source, commands, hashes and evidence must survive build cleanup.

Review VMs 61610, 6233 and 85869 remain live and untouched. One build/harness runs
at a time here; no physical disks, host reboot, Actions or merge request. Status:
implementation and proof complete; publish a scoped draft for review. The CLI journey uses the existing UX
recovery guidance: diagnose uncertainty before an explicit corrective action;
do not imply completion from a timeout or offer destructive retry.

### Publication-recovery contract and evidence

The new `publication.json` is created exclusively and flushed, including its
directory entry, only after image/GPT/ESP/partition verification and final source
checks. It binds the original plan hash, sources, target device/inode/UUID,
candidate device/inode/size/hash, partition hash and exact expected ready record.
It is an integrity record for an owned offline stage, not authentication.

`inspect` emits JSON with `state`, `can_reconcile` and `reason`. Exit zero means
diagnosis succeeded, not that an installation is ready. It never writes the
stage. `reconcile` takes a nonblocking cooperative lock on the stable plan inode,
rechecks everything, then may publish verified hard-link aliases and canonical
ready bytes. It never rebuilds GPT or replays provisioning. A matching ready
stage is idempotent. Unknown files, unsafe links, nonprivate ownership/mode,
changed sources/output or malformed metadata are preserved and refused.
Only known duplicate image/ready aliases of the same inode may be removed.

```sh
python3 tools/installation_image.py inspect .local/installation/profile-a
# Only when diagnosis reports can_reconcile=true:
python3 tools/installation_image.py reconcile .local/installation/profile-a
```

Keep the original stage and sources offline at their reviewed paths. Copying a
stage changes its identity and is deliberately refused; evidence backups are
audit copies, not adopted recovery targets. Legacy stages without this tool's
complete publication record cannot be retroactively reconciled. Once a guest
writes the image, its original ready receipt remains historical and recovery
refuses to overwrite or relabel it.

Clean source `291f469e` passes the full matched build and the real-process
proof below. Every apply/reconcile interruption uses a separate CLI process and
exit 75 after successful host I/O at a named boundary; no mocked kill is counted.

| Boundary / scenario | Observed outcome |
| --- | --- |
| Provisioning-started, provisioned, assembled | Read-only inspection reports incomplete; reconciliation refuses with the complete stage unchanged |
| Verified, image-linked, image-published, ready-written, ready-linked, ready-published | Reconciliation succeeds using identical candidate bytes; source inputs stay unchanged; repeated reconciliation is idempotent |
| Reconciliation interrupted after image-linked and ready-linked | Fresh processes diagnose each state and finish publication; no data re-provisioning or image rewrite |
| 16 altered-state cases | Plan plus checksum, target inode, bundle/driver/kernel/base, GPT/ESP/data, profile image, torn journal, unknown file, symlink, outside hard link and torn/wrong ready records all refuse without mutation |
| Reconciled profile boots | Two VM lifetimes pass edit/save/reboot/reopen, UUID/local binding, text/binary documents, all six portable settings, machine settings, environment/cwd and app binary checks |
| Guest-written image | Inspection/reconciliation refuse the changed image after both boots, preserving all bytes and the original ready receipt |

The [portable receipt](assets/screenshots/installation-recovery-manifest.json)
records boundary states/exit codes, whole-stage snapshot hashes, candidate hashes,
refusal preservation, restart/idempotency results and both boot readbacks.
The [reopened document](assets/screenshots/installation-recovery-reopened.png)
was visually inspected with the existing profile journey. Raw images, source
snapshots, commands/logs and matched artifacts are byte-verified outside build
at `.local/installation-evidence/recovery-d86be75i`. All task VMs are stopped;
original review VMs 61610, 6233 and 85869 remain live and untouched.

Host coverage includes the existing 27 installation/provenance tests plus eight
recovery groups, including the 18-case ordinary-v1/profile-v2 checkpoint matrix,
busy locking, private-stage admission, exact ready bytes and malformed candidate
types. Existing profile/session, 13 bundle groups and seven adapter tests pass.
Reproduce sequentially with the documented Darwin host compiler wrapper:

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py -D_DARWIN_C_SOURCE -Wno-array-bounds' test-installed-root test-profile-desktop check-docs
sh tools/macos/build.sh -j2 all
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd python3 tools/installation_recovery_smoke.py --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

No power cut, arbitrary write/fsync failure, physical media or guest fault
injection is qualified. PR322's managed writeback/retirement contract remains
separate; this tool does not clear its failure latches or PR320's profile import
markers. Torn metadata and unverified partial output stay untouched. External
`fsck.exfat` was not run; no automatic repair or in-place upgrade is provided.

## PR321 integrated-main review and next checkpoint

Final refresh: main advanced to `a8e1fca1` with merged guest profile PR320 during
merge preparation. Its pending-import startup check changes an exercised path,
so it was merged into candidate `d0910498` and the full proof was rerun.
All four VM boots, 12 installation refusals and 12 invalid session launches pass
again. Fresh 27 installation/provenance tests, profile host checks, 13 bundle
groups, seven image-adapter groups and 16 guest-profile groups pass, as do the
matched build and documentation checks. No implementation fix was required.
The [current review receipt](assets/screenshots/profile-install-current-review.json)
and [reopened document](assets/screenshots/profile-install-current-reviewed.png)
record this acceptance. Exact raw evidence/artifacts are byte-verified at
`.local/installation-evidence/pr321-current-tkf2agxs`. All task VMs are stopped;
the three original review VMs remain live and untouched.

The initial review below applies to the earlier `805519b2` snapshot only.
Historical accepted images were losslessly compressed with restoration paths
and original hashes in their retained `compressed-images.json` files.

User authorized review and merge if ready. Merged then-current main `805519b2` into
PR321 at `6de308a1`. No blocking implementation finding or unresolved review
thread remains. Fresh 27 installation/provenance tests, 13 bundle groups, seven
image-adapter tests, profile/session host checks, full build and documentation
checks pass. Shared profile preflight, plan-version guards, source/driver
revalidation and private publication preserve the accepted contracts.

The [equivalence receipt](assets/screenshots/profile-install-review-equivalence.json)
compares the fresh build with clean VM source `bf6efeb2`. Kernel, actual minimal
bootstrap init, loader, drivers and every exercised profile/application file are
byte-identical. Only unused `sysabi` lifecycle-test payloads changed: `bin/sysabi`
and its `bin-sysabi` alias in exFAT, plus `sysabi` and the embedded exFAT in the
ordinary full archive. The installer creates a minimal `/init` archive, so those
test payloads are not a changed boot dependency. All other archive entries match.
Thus no runtime behavior exercised by the accepted four-boot/12-refusal proof
changed; the affected host checks were rerun and that VM evidence remains valid.
Final image/refusal hashes were rechecked against the retained receipt.
Review logs and equivalence evidence survive at
`.local/installation-evidence/pr321-review`. Existing review VMs 61610, 6233 and
85869 remain live and untouched; no new test VM was needed. PR321 records the
final user-authorized merge state. Pending-publication statements below retain
the original milestone history.

Next bounded installation milestone: **interrupted image-publication recovery**.
Today apply refuses reuse of an interrupted stage, and an interruption after
image publication can leave a complete image without a ready receipt. Add
explicit inspect/reconcile for this owned disposable staging area. Persist the
verified output identity/hashes before publication; a fresh process may finalize
ready state only when plan, sources, target identity and the complete output all
still match. Otherwise preserve the stage and refuse; never resume arbitrary
writes into an uncertain image.

Prove process interruption at provisioning, GPT assembly, image publication and
ready publication boundaries; successful reconciliation must boot and retain
the planned profile, while changed/damaged inputs or output cannot become ready.
Dependencies: version-2 plan/source binding from PR321, existing profile-image
clean admission/private provisioning from PR314, PR320's pending-import and
journal ownership rules (host installation recovery must not clear guest profile
markers), stable GPT/boot verification
and explicit host write/flush/directory-sync ordering. Agree any new persistence
assumptions with the filesystem owner; process-restart proof is not guest
power-loss atomicity. Guest profile import/export, physical disks, credentials
and in-place upgrades remain separate. This is a proposed next slice only.

## Profile-aware installation plans — completed proof

User authorized implementation, two-installation persistence/refusal proof and
a scoped PR. Branch `device/apple-silicon/profile-install-plans` uses the existing
isolated installation clone, based on fetched main `c7be6574` (merged filesystem
acceptance PR318). Latest contracts consumed: merged PR309 bundle/local identity,
PR314 offline image adapter and explicit profile session (Apps `3cf32af2`),
PR313 namespace/clean admission and merged Ports `5e994f53`.

Own installer plan/API/tests, a new profile-installation proof and this record.
Reuse profile validation/publication and explicit desktop startup; no competing
account store, home resolver, namespace or credentials implementation. A minimal
shared profile validation helper keeps prepare and provisioning under the same
contract. Filesystem busy-unmount/remount work is independent; this slice does
not edit the installed-root/canonical harnesses or guest namespace tests.

Required outcome: prepare validates profile bundle/home/local alias/UID/GID before
creating a target; a checksummed plan binds source bundle and driver identity,
portable UUID and destination binding. Apply provisions a private exFAT copy,
assembles the new identity-bound GPT image and verifies its exact data bytes
before ready publication. Explicit profile launch remains separate from boot.
Original inputs are preserved; malformed, changed or interrupted inputs must not
publish an installed image/ready marker.
Profile plans use version 2 so legacy version-1 apply refuses rather than silently
ignoring profile fields. Ordinary plans remain version 1; version downgrade or
profile/version mismatch refuses before provisioning. Checksums detect accidental
edits, not an adversary replacing both a plan and its checksum.

Acceptance: A edit/save/reboot, export the actual persisted content, then B
provision/reboot with the same UUID and different disk GUID/home/local bindings;
exact documents, all six portable values, environment/cwd and independent machine
settings; guest/offline allocation checks and protected regions; real refusal
fixtures plus focused unit tests. Source/artifact provenance and all raw evidence
must be retained outside build before delivery. Status: implementation and proof
complete; publish a scoped draft for review.

Review VMs 61610, 6233 and 85869 remain live and untouched. One build/harness runs
at a time here; no physical media, host reboot or Actions. No guest import UI,
authentication or in-place update is included.

### Profile-plan interface and acceptance

`prepare` accepts the all-or-none options `--profile-bundle`, `--profile-home`,
`--profile-alias`, `--profile-uid`, `--profile-gid`, `--profile-driver`, together
with `--identity-bound`. The supported destination is `/fat/home/<name>` under
the profile owner's existing length/path rules. All profile validation is shared
with `profile_image.validate_request`; prepare does not invent another schema.
The plan stores the profile UUID and local binding, plus immutable-source
identities/hashes for the bundle, driver and base image. Apply revalidates them,
provisions `profile.exfat` privately and verifies the installed partition against
that exact provisioned output. `ready.profile` reports the explicit guest launch
command; `ready.profile_exfat_sha256` records the provisioned content. No desktop
session is selected automatically by boot.

Example, using a validated bundle and the matched host provisioning driver:

```sh
python3 tools/installation_image.py prepare .local/installation/profile-a \
  --kernel build/kernel/srvros.elf --bootx64 build/limine-binary/BOOTX64.EFI \
  --exfat build/srvros.exfat --init build/userspace/init.elf --identity-bound \
  --profile-bundle /absolute/path/profile.json --profile-home /fat/home/local-a \
  --profile-alias local_a --profile-uid 1001 --profile-gid 1001 \
  --profile-driver build/tests/profile-image-driver
# Review plan.json, then apply that unchanged plan:
python3 tools/installation_image.py apply .local/installation/profile-a
```

Clean tested source `bf6efeb2` passes the matched full build and four VM boots:
A edit/save, A reboot/export of actual persisted content, B installation from
that export, and B reboot. A uses `/fat/home/local-a`, local metadata 1001/1001;
B uses `/fat/home/local-b`, 2050/2051. All GPT disk/partition IDs differ, while
the portable UUID remains the same. Both installations use profile-aware
version-2 prepare/apply, not a separately pre-provisioned profile image.

| Check | Result |
| --- | --- |
| Documents and preferences | Edited text and binary document are exact on all four readbacks; all six portable values match |
| Machine-local state | Each complete machine settings file remains equal to its own seed; local bindings differ as requested |
| Active session | Files opens the selected home; Text/pickers reopen its document; Terminal environment and cwd match all HOME/XDG/profile values |
| Boot and applications | Target-only EFI boot; identity selection precedes namespace activation and explicit profile launch; three installed app binaries match the build |
| Filesystem integrity | Four guest and four offline allocation checks clean; protected regions and source images unchanged |
| Prepare refusals | Malformed/truncated bundle, partial binding, invalid alias/UID, traversal home refuse before target creation |
| Apply refusals | Changed bundle/plan/driver and rechecksummed version downgrade refuse with no image/ready publication |
| Backend refusals | Existing profile home and failing executable driver reach actual provisioning refusal; no publication or source changes; interrupted stage cannot be resumed |
| Session refusals | Three invalid profile selections per boot return to the monitor rather than selecting a shared home |

All **12** installation-refusal cases preserve their base, bundle and driver
inputs after the fixture's intentional tamper step. No refused case publishes
`image.img` or `ready.json`. The [portable receipt](assets/screenshots/profile-install-manifest.json)
records source/build/tool/firmware hashes, plans' binding metadata, four boot
readbacks, refusal hashes, protected regions and evidence hashes. Raw evidence
and exact artifacts are byte-verified outside build at
`.local/installation-evidence/profile-plans-l1_8_9zl`. The earlier successful
version-1 trial at `2a3c79f8` is retained as history with verified compressed
images at `.local/installation-evidence/profile-plan-v1-1xe4uuvn`; final review
introduced version 2 to make older installers refuse unsupported profile plans.

[B's reopened Text document](assets/screenshots/profile-install-text-b2.png) and
[its Terminal session](assets/screenshots/profile-install-terminal-b2.png) were
visually inspected as an expert walkthrough. No user study is claimed. Shared
desktop proof now waits for window publication instead of an early startup log;
actual open/save/clipboard and disk-byte checks remain mandatory.

Validation: 27 installation/provenance tests, 13 bundle test groups, seven image
adapter tests and the existing profile/session/preferences host checks pass.
Run `sh tools/macos/build.sh` with the documented Darwin host compiler wrapper
for `test-installed-root test-profile-desktop check-docs`, then build `-j2 all`.
Run the exact-source proof with:

```sh
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd python3 tools/profile_installation_smoke.py --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

Filesystem PR319 landed during validation at `805519b2`; its optional workspace
lifecycle fixture changes neither profile provisioning nor these contracts.
This proof uses the named `c7be6574` baseline. Kernel, credential, namespace and
physical-disk policies are unchanged. UID/GID values are local metadata; bundle
hashes provide integrity, not authentication. Host publication guarantees do not
establish guest power-loss atomicity, arbitrary I/O-fault recovery or upgrade
rollback. External `fsck.exfat` was not run. All task VMs are stopped, and the
three original review VMs remain live and untouched.

## PR315 integrated-main review and next milestone

PR313 landed as main `56e1f950`, including PR314 desktop profiles, PR252 and
the merged SrvrPorts dependency. User authorized review and merge of PR315 if
ready. Merged that main into the delivery branch and rebuilt the complete matched
image. Review found a harness readiness problem: the early Text startup log was
absent even though Text published its window. The harness now waits for window
publication; actual save/reopen responses, clipboard length and disk bytes remain
required. The initial timeout is retained outside build at
`.local/installation-evidence/pr315-startup-log-timeout`.

Clean reviewed source `57fcf482` passes all 11 fresh VM cases against integrated
main, including canonical Text/theme persistence after reversal and nine recovery
cases preserving both full disk hashes before namespace activation. Focused
storage, installation/provenance, namespace/syscall and profile/session/image
host tests pass; full build and documentation checks pass. No unresolved review
threads were present. The [review receipt](assets/screenshots/installed-canonical-review-manifest.json)
records exact identities/hashes and the verified backup at
`.local/installation-evidence/pr315-5wv8f9vm`; the
[reviewed screenshot](assets/screenshots/installed-canonical-reviewed.png) shows
Text reopening `/home/installed-proof.txt`. All task VMs are stopped; existing
review PIDs 61610, 6233 and 85869 remain live and untouched. Ready for the
user-authorized merge; PR315 is the authoritative merge record. Earlier pending
dependency statements below describe the initial publication only.

Next bounded milestone: **profile-aware disposable installation plans**. Compose
the existing installer and offline profile adapter so one reviewed plan binds a
validated portable bundle to an explicit destination home/local alias/UID/GID,
then publishes a new identity-bound GPT image. Prove explicit profile launch,
document and portable-setting persistence after reboot, and the same portable
UUID across two installations with different disk GUIDs and local bindings.
Malformed bundles or changed plans must refuse publication and preserve inputs.

Dependencies are now merged: PR309's bundle/local-binding contract, PR314's
`profile_image.py` and clean `/fat` metadata publication plus SrvrApps5 session
consumer, and PR313's canonical namespace/lifetime contract. Reuse these APIs;
do not create another profile store or namespace. The current accepted profile
home is `/fat/home/<name>`; logical `/home` aliases do not expand that startup
allowlist automatically. Preserve machine settings independently of the six
portable fields. Guest import UI, login/authentication, physical media and
in-place upgrades remain later work. This paragraph plans the next milestone;
it does not begin implementation.

## Canonical-path installed boot — completed proof

User authorized the canonical-path installed-boot proof and a scoped PR. Work
uses integration branch `device/apple-silicon/installed-canonical-paths` in the same isolated clone,
from merged PR312 main `8f47f196`, consuming exact PR313 `06d5a416` by a local
history-preserving dependency merge. PR313 still awaits its separate integration
review; this branch does not authorize merging that PR or its dependencies.
It includes PR308, full PR252 `badfeb37`, and SrvrPorts pin `0f51ba9`.
The delivery branch `device/apple-silicon/canonical-install-proof` contains only
this milestone's harness/documentation/evidence commits on main `9d642c75`.
Its draft PR depends on PR313; the published integration branch retains the
complete tested source without republishing the dependency's implementation in
the delivery diff. Do not merge the delivery PR until PR313 lands and the
combined source is reconciled against this evidence.

Latest profile contracts checked: PR309 `fa9717ad` and newer desktop-adoption
PR314 `814aee20` (SrvrApps5). Profile sessions are explicitly selected and remain
distinct from root credentials, disk identity and namespace activation. This
slice retains the existing home provider and desktop settings. PR309 then landed
as main `9d642c75`, which is merged here before the matched build; PR314's desktop
session implementation remains separate.

Own the installed GPT fixture/harness and this acceptance record. Consume the
filesystem owner's `/bin` → `/fat/bin`, `/usr` → `/fat`, `/var` → `/fat/var`,
`/home` → `/fat/home` implementation unchanged. Required evidence: target-only
EFI boot, identity selection before aliases, reversed GPT disks, canonical and
legacy path identity/execution, Text/preferences persistence, an independent
GPT-backed `/work`, and no-write recovery before namespace activation. Positive
boots intentionally write the independent provider; protected disk regions must
stay unchanged. Refusal cases must preserve both complete disk hashes.

Existing review VMs 61610, 6233 and 85869 remain live and untouched. One build or
harness runs at a time in this clone. No physical install, host reboot, Actions,
profile activation or upgrade/rollback is included. Status: implementation and
combined proof complete; publish as a draft while dependency integration remains
separate. No dependency PR or current milestone merge is authorized by this run.

The initial `77dbd1f9` dependency passed host checks and positive namespace VM
checks, but its run at `b8da407c` was stopped when PR313 published path-limit and
rename-error fixes. The retained `proof-8_v1flfj` is superseded, not final
acceptance. It is retained with verified gzip disk copies at
`.local/installation-evidence/canonical-superseded-8_v1flfj`. The final proof
rebuilt and reran with `06d5a416`.

### Canonical-path acceptance

Clean source `3e47911945967e3bb96279938e1199a35b26b730` passes the matched full
build and all **11** Q35/AHCI/OVMF VM cases. The
[portable receipt](assets/screenshots/installed-canonical-manifest.json) records
source/submodule identities, build/tool/firmware hashes, disk GUIDs, case results,
protected regions and log/screenshot hashes. Later documentation commits do not
change that tested implementation.

| Proof | Result |
| --- | --- |
| Initial EFI boot | Target selected as `ahci0p2`, then aliases activate, then init becomes ready; independent `ahci1p2` mounts at `/work` |
| Reordered fresh boot | Same target GUID selected as `ahci1p2`; independent provider is now `ahci0p2` |
| Canonical identity and operations | `/bin`, `/usr/bin`, `/home`, `/fat/home` and `/usr/home` checks pass for shared identity/content, execution, cwd/dirfd, creation/mutation guards, path limits and rename errors |
| Persistence | Different home/work markers survive reboot and verify through aliases, then clean up; Text reopens exact document bytes via `/home/installed-proof.txt`; preferences match |
| Isolation and integrity | `/work` is distinct from system/home; both disks retain all bytes outside their data partitions; guest system and four offline system/work allocation checks are clean |
| Nine recovery cases | Missing/wrong/duplicate identity, wrong role/type/format, invalid map, damaged exFAT backup and pending metadata all halt before aliases/init/monitor; both whole-disk hashes remain unchanged |

The proof uses two independently generated GPT images, the target's own EFI
loader/kernel/minimal `/init` archive, and no ISO or host kernel/initrd. `/work`
is explicitly mounted from the generated other partition by the harness; this
does not qualify automatic USB workspace discovery or introduce a home-provider
selection policy. Positive boots intentionally change both data volumes. The
nine recovery cases compare complete images, not merely protected regions.

[Text reopened through `/home`](assets/screenshots/installed-canonical-reopened.png)
and [recovery before aliases](assets/screenshots/installed-canonical-recovery.png)
were visually inspected as an expert walkthrough. File opening, selected text,
restored theme and halted diagnostic are visible. This reuses the existing
desktop/recovery journey; no representative-user study or new UI is claimed.

Raw images, commands, UART/QEMU logs, screenshots and exact built artifacts are
retained outside `build` at `.local/installation-evidence/canonical-e5t4y5zz`.
Backup hashes, final disk hashes and normal/recovery startup ordering were
independently verified. The task VMs are stopped; original review VMs 61610,
6233 and 85869 remain live and untouched.

Host validation passes: storage and clean-mount checks, 21 installation/provenance
tests, namespace/syscall/process/loader/service/lifetime checks, real
process/exFAT writeback, and all 133 filesystem/namespace modes with ASan/UBSan.
The selector sanitizer also passes. The initial Make process-writeback invocation
inherited Zig's cross-compiler and failed on the host CPU; the direct x86_64 host
invocation below passes. That initial log is retained, not counted as a pass.

Reproduce at the published integration source `3e479119` in an isolated checkout,
sequentially with the documented Darwin host compiler wrapper. The delivery
branch alone awaits PR313 and is not the combined runtime used for this proof:

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py -D_DARWIN_C_SOURCE' test-storage test-installed-root test-vfs-namespace test-syscall-namespace test-process-output test-process-loader test-process-service test-process-lifetime
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --process --output build/review/installed-canonical-process
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --sanitize --output build/review/installed-canonical-sanitize
python3 tools/test_installed_root.py --sanitize
sh tools/macos/build.sh -j2 all
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd python3 tools/installed_root_smoke.py --canonical-paths --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
sh tools/macos/build.sh check-docs
```

PR312's limits still apply: GUID selection is not authentication; primary GPT
only; unknown raw maps refuse; OVMF may repair a primary GPT before kernel entry,
so the invalid-map fixture damages both copies. Missing system volume retains
the bootable ESP. No physical power-loss, arbitrary mount-I/O-failure, native
hardware, profile/session activation or external `fsck.exfat` qualification is
claimed. The profile foundation is present from main; PR314's desktop adoption
and SrvrApps5 remain independent of this canonical-path proof.

## PR312 integration review and next checkpoint — November 3, 2026

User requested review and merge if ready. Integration review found no blocking
defect in [PR312](https://github.com/PaulMDemers/SrvrOS/pull/312) against main
`3db1a1d3`, filesystem proof PR308 `88ea5524`, newer global activation PR313
`29a0ad23`, and profiles PR309 `fa9717ad`. The two filesystem PRs and the profile
PR are still drafts; this review does not merge them or claim combined runtime
acceptance. PR313 attaches its aliases after `/fat` and bootlog setup, so PR312's
identity admission and recovery halt remain earlier in the boot sequence.
Neither filesystem branch supplies a competing installed-volume selector.

Rechecked the clean tested source `9989878c` against the PR: all later changes
are documentation/evidence only. Retained build artifacts, logs, screenshots and
all final disk hashes match the 11-case receipt; the nine recovery logs contain
no normal startup. Fresh storage, selector, 21 image/provenance, clean-mount,
ASan/UBSan and documentation checks pass. The existing full-build and VM evidence
remains applicable; no new VM was needed for unchanged implementation. No review
threads or failing commit statuses were present; hosted Actions remain disabled.
The three original review VMs remain live and untouched. Readiness: ready for
the user-authorized merge; the linked PR records the final merge state.

Next bounded installation milestone: **canonical-path installed boot in QEMU**.
Combine identity-selected boot with the filesystem owner's canonical namespace,
then prove `/bin`, `/usr`, `/var` and `/home` resolve to that selected system while
legacy `/fat` and an independent data provider stay distinct. Use disposable
disks, reverse enumeration, execute/read/write through canonical and legacy
names, reboot and reopen the same document/preferences. Missing or ambiguous
system identity must still halt before namespace activation or disk writes.
This closes the gap between the separately tested installed-root and global-path
proofs; it does not add physical installation, upgrade slots or profile import.
PR313's present harness boots an ISO with raw exFAT disks. The combined fixture
must instead boot the target's EFI payload and use valid GPT maps on every raw
disk, mounting any independent provider by its selected partition. Do not weaken
PR312's refusal policy to reuse the raw-disk fixture unchanged.

Dependencies: integrate/review PR313 with its PR308 foundation, full PR252
process/I/O lifetime handoff and SrvrPorts PR3 pin `0f51ba9`; preserve one canonical
backing identity for handles, cache, mutations and unmount checks. Consume the
actual merged revisions rather than copying the namespace implementation into
installation tooling. Profiles PR309 supplies the HOME/config/data/cache contract
and keeps portable UUID separate from disk GUID and local UID/GID. For this slice
retain the explicit `/fat/home` provider behind `/home`; guest login, profile
import and the portable-settings adapter are not prerequisites. A later import
milestone needs destination-local binding, guest-qualified publication/recovery
and application adoption; the host fixture's rename guarantee is insufficient.

## Identity-bound boot milestone — November 3, 2026

User authorized implementation and a scoped PR with the two-disk proof. Base is
merged PR311, fetched main `3db1a1d3`; branch
`device/apple-silicon/installed-root-identity` uses the existing isolated
installation clone. Original review VMs 61610, 6233 and 85869 remain live and
untouched. One build/harness runs at a time here; no physical media is a target.

Latest contracts remain filesystem PR308 `88ea5524` and profiles PR309
`fa9717ad`, both open. No selector implementation exists in either diff. This
branch proposes the missing opt-in installation selector using the existing GPT
validator and exFAT mount. Shared selector/mount integration needs filesystem-owner
review before merge; generic namespace attachment remains PR308's ownership.
No change to portable identity, HOME policy, credentials or process/I/O lifetime
is included. PR252's block/main diff is empty; its independent FAT sizing fix
is preserved by adding only optional GUID/name inputs to the image builder.
PR301's native display hooks in main remain separate from this boot decision.

Proposed opt-in contract: `srvros.root=<canonical GPT partition UUID>`,
`srvros.root-format=exfat-v1`, `srvros.root-role=system`. The selected partition
must have the basic-data GPT type and exact role name `srvros-system-v1`, with
exFAT validated before normal use. The provisioner chooses disk/partition GUIDs
before writing boot configuration and binds all values into the reviewed plan.
The opt-in clean mount performs no writes or flushes during admission. It refuses
filesystem recovery flags, invalid boot copies, pending metadata temporaries and
malformed committed metadata; it never replays a sidecar while selecting a root.
Ordinary mounts keep their existing behavior.
No selector fields means the ordinary legacy boot path. Partial, duplicate or
malformed selector fields refuse; any unknown/invalid participating raw map
refuses rather than assuming it cannot contain a duplicate. GUIDs establish
selection, not authentication or resistance to a deliberately cloned identity.

Acceptance: target plus decoy, reversed device ordering, exact saved document
and preferences after reboot, and full decoy hash preservation; missing, wrong,
duplicate and corrupt candidate refusal with full hashes of both disks unchanged.
The target alone supplies its boot payload throughout. Recovery uses the existing
framebuffer/UART to show a reason and correction path, then halts before bootlog,
init, workspace startup or the writable monitor. It is a diagnostic recovery
halt, not a repair shell or automatic upgrade rollback. This flow applies
optimize-ux's explicit failure/correction states, checked in real VMs.

Status: implementation and disposable VM proof complete; initially published as
a draft, then reviewed for the authorized integration above. Physical install, in-place upgrade,
profile import and guest account activation remain out of scope.

### Accepted two-disk evidence

Clean tested source `9989878c` (full SHA in the
[portable receipt](assets/screenshots/installed-root-manifest.json)), based on
merged PR311 `3db1a1d3`, passes all 11 VM cases on Q35/AHCI, x86_64 TCG and OVMF.
The full matched build includes the exact source/submodule states and artifact
hashes; bootstrap `/init` and installed desktop binaries were checked against it.

| Case | Observed result |
| --- | --- |
| Target first, decoy second | Selected `ahci0p2`; Text saved a document and Settings saved the theme |
| Order reversed, fresh VM | Selected the same GUID as `ahci1p2`; Text reopened exact bytes and preferences matched |
| Missing system entry or wrong partition GUID | `missing` recovery halt |
| Duplicate partition GUID | `ambiguous` recovery halt |
| Wrong GPT role name or type | `role` recovery halt |
| Wrong exFAT signature | `format` recovery halt |
| Invalid decoy GPT copies | `invalid-map` recovery halt |
| Invalid exFAT backup boot region | `filesystem-recovery` halt |
| Valid pending `.srvros/meta.tmp` | `filesystem-recovery` halt without replay |

Every recovery case hashes **both complete disks** before and after QEMU exits;
all pairs match. Each positive boot leaves the decoy unchanged and preserves all
target bytes outside exFAT. Guest and offline allocation checks report no leaks,
FAT/bitmap mismatches or errors. Recovery is observed before stopping only the
test-owned VM, with no init, monitor, fallback or persisted bootlog. The only
boot payload is on the target ESP; no host kernel/initrd, ISO or RAM app image is
attached. The missing-volume case retains that ESP, because removing the sole
boot disk exercises firmware failure, not kernel recovery.

[Reopened Text](assets/screenshots/installed-root-reopened.png) and
[duplicate-identity recovery](assets/screenshots/installed-root-recovery.png)
were visually inspected, along with pending-metadata recovery. The recovery
message states the reason, that applications have not started and writes are
stopped, and tells the operator to check the manifest/disks and restart the VM.
This is a developer recovery walkthrough, not a representative-user study or
an interactive repair utility.

Raw disks, QEMU commands/logs/screenshots, receipt and exact build artifacts are
retained under `.local/installation-evidence/proof-p0_hk8ic`, outside `build`.
The backup was compared byte-for-byte by SHA-256; final disk hashes were checked
against the receipt. Original review VMs 61610, 6233 and 85869 remain live and
untouched; all milestone-owned test VMs are stopped.

### Reproduce and review boundaries

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-storage test-installed-root
python3 tools/test_installed_root.py --sanitize
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-exfat-import test-exfat-boot test-exfat-crash
sh tools/macos/build.sh -j2 all
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd python3 tools/installed_root_smoke.py --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
sh tools/macos/build.sh check-docs
```

The Darwin host-compiler wrapper is described in [macOS development](macos-development.md).
Use one build/harness at a time; `test-installed-root` includes 21 Python image
and provenance tests, the selector test and real exFAT clean-admission checks.
The latter prove clean acceptance, pending/malformed metadata and damaged-backup
refusal with zero write/flush calls. Existing GPT/block/AHCI, exFAT import,
boot-copy and legacy rename/replace/replay interruption regressions pass.
ASan/UBSan selector checks pass. No hosted Actions were run.

An earlier trial at `b8a3fc87`, retained in
`.local/installation-evidence/proof-2_fhjfpx`, deliberately corrupted only the
decoy's primary GPT CRC. OVMF repaired it from the valid backup before kernel
entry; SrvrOS then saw a valid map and booted. That trial is **not** a recovery
pass. The accepted invalid-map fixture damages both copies to prevent firmware
repair. Kernel no-write admission cannot promise firmware will never write a
disk before handing off control. The selector currently validates primary GPT
only; it does not validate/repair backup GPT or authenticate GUIDs. Unknown raw
media conservatively refuses the installation. This is neither physical
power-loss qualification nor arbitrary mount-I/O-failure qualification; external
`fsck.exfat` was not run.

Filesystem PR308 `88ea5524` must review the shared identity discovery and clean
mount API before integration; its generic namespace aliases are not activated
here. Profiles PR309 `fa9717ad` remains a contract dependency for future import:
portable UUID is distinct from disk GUID/local UID/GID, and destination
HOME/config/data/cache must be supplied independently. Existing `/fat/home`
persists unchanged in this milestone; no guest profile consumer is required.

The remaining sections retain the PR311 single-disk checkpoint and merge review
as history. Their proposed next milestone is the completed proof above.

## PR311 target and ownership (historical)

The first checkpoint is an independently bootable, disposable QEMU disk: its
own EFI bootloader, kernel and bootstrap, applications loaded from its exFAT
partition, and a document/preferences verified after a fresh boot. No ISO,
host `-kernel`/`-initrd`, RAM exFAT fallback or second data disk may satisfy it.
This is image provisioning for a VM, not a physical-disk installer.

Source baseline: fetched `origin/main` `a936baeb` on November 3, 2026. Work uses
the isolated `SrvrOS-installation` clone and branch
`device/apple-silicon/disk-installation`. The primary checkout and review QEMU
PIDs 61610, 6233 and 85869 were observed live and are preserved. Actions remain
disabled; no native disk writes or host reboot are authorized here.

Own: installation manifests, target preflight, disposable image assembly,
installed-disk harness and this document. Filesystem/root namespace policy is
owned by the filesystem chat; stable identity and portable homes are owned by
the profiles chat. Consume their published contracts before changing root or
account policy. Existing `userspace/install` copies files; it is not this tool.
Inspected live PR heads: PR252 `badfeb37`, PR19 `993e920a`, PR301 `4c9fc184`,
PR306 `5028871b`. PR252 also changes the FAT builder sizing loop; reuse the
existing fixed 384 MiB ESP without editing that shared helper. No competing
kernel, libc, process lifetime, Wi-Fi or display implementation is included.

The filesystem owner's published contract at
[`6686ae18`](https://github.com/PaulMDemers/SrvrOS/blob/6686ae18fc709a07b636b97fac9e4161a2f3b9da/docs/filesystem-layout.md)
explicitly permits consuming the existing exFAT `/fat` image for this checkpoint.
Its `/bin`, `/usr`, `/var` and `/home` namespace view is an opt-in proof, not a
new boot root. This installation does not depend on those aliases. The required
next interface is an unambiguous stable volume identity plus format/version and
role, with recovery on mismatch instead of discovery-order fallback.

## Boot and storage contract

The existing kernel loads `/init` from a Limine bootstrap tar, discovers
`ahci0` then `virtio0`, and tries GPT partition 2 first for `/fat`. It can
otherwise fall back to a RAM `/srvros.exfat`. The installation proof uses a
minimal bootstrap with `/init` only, excluding that fallback and app binaries.
The bootstrap remains in memory after the bootloader reads it from the target;
this does not make application/data storage a RAM root.

| Role | First image | Contract / limitation |
| --- | --- | --- |
| Disk identity | New GPT disk GUID and private staging identity | Regular-file fixture only; no `/dev` or unknown existing target |
| Boot | Partition 1, FAT32 ESP at LBA 2048, 384 MiB | EFI fallback path, Limine, kernel and minimal bootstrap on target |
| System applications | Partition 2, exFAT `/fat/bin` | Matched build image; currently shares volume with writable data |
| Data/state | Existing `/fat/home` and `/fat/var` | Existing namespace only; no new profile format or isolation claim |
| Root | Bootstrap VFS plus existing `/fat` mount | Stable root selection by GUID remains a filesystem-owner dependency |

The proof attaches exactly one storage disk. Enumeration-based selection is
insufficient for a multi-disk installer: require a filesystem-owned explicit
root identity contract before expanding. exFAT preserves current compatibility
but is not journaled; it does not provide crash-atomic upgrade transactions.

## Selection, completion and recovery flow

This new flow was reviewed using optimize-ux as a developer CLI journey. No
representative-user study or physical install UI has been performed.

| State | Information and action | Failure / recovery |
| --- | --- | --- |
| Prepare disposable target | Choose a new private staging directory; show source hashes, target identity and exact partition bounds | Existing, source-alias, symlink and unknown targets are refused; choose a fresh directory |
| Review plan | Inspect boot/system/data roles, capacity and the VM-only scope | A changed source or plan requires a new plan; no silent retargeting |
| Apply | Validate identities and bounds again, assemble unpublished image, verify and flush | Interruption never advertises readiness; retain evidence and prepare a fresh fixture |
| Boot proof | Firmware boots the single target, loads an application and saves data | Missing mount, init or app evidence is failure; retain image/logs |
| Reopen proof | Start a fresh VM from the same disk and verify exact bytes | Success requires matching saved content, not just a boot prompt |

For later physical installation, disk choice must show model, capacity, stable
hardware identity, existing partitions and source-media identity before the
destructive action. Boot source, mounted/in-use, ambiguous or changed targets
must be excluded. A name such as `disk0` or a list position is not consent or
identity. This physical workflow is specified, not enabled by these tools.

## Upgrade and recovery direction

The initial tool creates new images and refuses in-place upgrade. Keep the
previous disk/image and recover data read-only from a failed candidate. A
future recoverable upgrade needs separately versioned boot/system slots with
verified manifests, a retained known-good boot choice, and an explicit
successful-boot acknowledgment before retiring the old slot. Stage the inactive
slot, verify every artifact, then change only the boot choice; never overwrite
the only working boot payload or migrate homes irreversibly before acceptance.

Slot selection and interrupted boot-choice writes must be tested against the
actual firmware/filesystem contract. On FAT/exFAT, rename alone is not proof of
power-loss atomicity. Recovery must tolerate an incomplete slot/selector and
offer the prior verified release. Profile migration/export belongs to the
profile owner; the installer must preserve its versioning and rollback rules.
These upgrade/recovery requirements are a design contract, not an implemented
A/B updater or physical power-cut qualification.

## Reproduce the disposable proof

Use an independent checkout, initialize pinned submodules and build as documented
in [macOS development](macos-development.md). No host filesystem mounts or
privileged device tools are needed. Run one build/harness at a time:

```sh
sh tools/macos/build.sh -j2 all
python3 -m unittest discover -s tools -p test_installation_image.py -v
python3 -m unittest discover -s tools -p test_installed_disk_smoke.py -v
mkdir -p build/tooling
cat > build/tooling/darwin-host-cc.py <<'PY'
import os, sys
os.execvp('cc', ['cc', '-arch', 'x86_64'] +
          [arg.replace('-Wl,--gc-sections', '-Wl,-dead_strip') for arg in sys.argv[1:]])
PY
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' build/tests/exfat-driver
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/installed_disk_smoke.py \
  --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
sh tools/macos/build.sh check-docs
```

The QEMU installation's `share/qemu/firmware/60-edk2-x86_64.json` identifies the
matching code and variable-store templates on this host. Elsewhere supply the
matching firmware paths explicitly. Every boot gets a fresh private copy of the
variable template: this is firmware state, not a second OS/data disk. Code flash
is read-only. Neither flash contains a SrvrOS kernel or application. A fresh
variable store on the second boot also proves the EFI fallback boot path works
without retaining a host-created boot entry.

The harness prints its unique evidence directory under `build/review/installed-disk`.
It creates its own new target, runs two normal boots and a separate damaged-volume
control, then stops only its own VMs. Retained artifacts include the disk,
firmware variables, source/build records, boot commands, UART logs, screenshots,
extracted data partitions and allocation checks. Copy wanted evidence outside
`build` before `distclean`. Each `ready.json` describes the freshly provisioned
image hash; guest writes intentionally change the image afterward, recorded in
the proof receipt. Do not treat the provisioning hash as a current disk hash.

To review a plan separately from applying it, use a new directory name:

```sh
mkdir -p .local/installation
python3 tools/installation_image.py prepare .local/installation/trial \
  --kernel build/kernel/srvros.elf --init build/userspace/init.elf \
  --bootx64 build/limine-binary/BOOTX64.EFI --exfat build/srvros.exfat
cat .local/installation/trial/plan.json
python3 tools/installation_image.py apply .local/installation/trial
```

`prepare` refuses existing targets and symlink path components. `apply` binds to
the recorded staging device/inode, source identities/hashes and exact bounds;
it refuses unknown contents or changed sources. A separately saved `plan.sha256`
detects edits to the reviewed plan, including replacement source records and
target UUIDs, before any image writes. This detects accidental edits, not an
actor deliberately changing both the plan and its checksum. It verifies both GPT copies,
boot artifact readback and the copied exFAT before publishing `ready.json`.
Staging UUID and generated GPT disk/partition GUIDs are distinct and recorded.
An interrupted directory is retained and cannot be reused. These protections
cover accidental misuse of disposable fixtures; they are not a privileged,
hostile-concurrency-resistant physical installer or a power-loss durability claim.
The proof harness requires `build/macos-build.json` from the build wrapper and
refuses missing/malformed records, artifact mismatches, or a changed source tree
(including commit, tracked/untracked changes and recursive submodules). Re-run
the build wrapper after any source changes, even documentation-only changes,
before generating a new proof receipt.

## Evidence and next action

Status: the disposable installed-disk checkpoint is implemented and VM-tested.
Clean source `f0824d5c53252712e527d0a7462c3a4470ee379d`, based on `a936baeb`,
passes the complete proof with QEMU 11.1.2 on this ARM64 macOS host. Main was
fetched again before publication and remains `a936baeb`. The kernel and source
exFAT hashes match the first baseline build; no kernel/VFS/app code changed.
The documentation/evidence follow-up does not claim another runtime revision.

[Portable receipt](assets/screenshots/installed-disk-manifest.json) records
source/submodule pins, tool and artifact hashes, partition GUIDs and bounds,
firmware identity, check results and retained failures.
[The second-boot screenshot](assets/screenshots/installed-disk-reopened.png)
was inspected: Text shows the exact reopened document under the restored Aero
theme. The proof image is 514 MiB: a 384 MiB ESP and 128 MiB exFAT volume, plus
alignment and partition tables. The screenshot is supporting visual evidence;
exact bytes and preference restoration are independently checked in the harness.

| Required outcome | Actual evidence |
| --- | --- |
| Boot from installed target | Firmware reports the QEMU hard disk; Limine loads both kernel and bootstrap from `boot():`; each command has one storage disk, no CD-ROM or host kernel/initrd |
| Disk-backed applications | Only `/init` is in bootstrap; `/fat` mounts from `ahci0p2`; installed deskd/desktools match built runtime ELFs; Text and Settings run |
| Write and reboot persistence | Text writes `installed disk persistence proof`; Settings saves `theme_style=1`; normal guest reboot flushes storage and exits QEMU with `-no-reboot`; a new VM reads exact bytes, restores theme and reopens/selects the document in Text |
| Selected bounds | GPT/boot readback validates exact layout before publication; hashes of every byte before and after the data partition remain equal after both boots |
| Filesystem consistency | Both guest checks report 365 files, 13,821 clusters, zero leaks/FAT errors; both x86_64 host allocation checks report zero leaks, mismatches or errors |
| No live-image false pass | Separate copy with both exFAT boot sectors invalid reaches explicit missing-fallback diagnostics and cannot find the installed application ELF |
| Preflight/interruption refusal | Eleven Python fixtures pass: existing/source/alias/symlink/unknown target, changed source, malformed geometry/bounds and tampered plan, GPT corruption, interrupted build/publication and successful minimal-bootstrap publication |
| Repository verification | Matched `all`, Python compilation, documentation links and `git diff --check` pass; Actions remain disabled |

The first trial correctly failed the protected-region check because unpaired
OVMF wrote `NvVars` into the ESP. Pairing the code flash with the matching blank
variable-store template resolved this without relaxing the check. Two subsequent
harness failures (picker-list focus and early consumption of an echoed prompt)
are retained as failed attempts, not counted as passes. The final run is
`proof-ris6lu8z`. Full accepted evidence is copied to
`.local/installation-evidence/proof-ris6lu8z/`, surviving `distclean`; validation
logs are under `.local/installation-evidence/validation/`. Earlier failed runs
remain under `build/review/installed-disk/`.

Limitations: this is one clean reboot of one disposable image, not physical
power-loss acceptance, full recovery qualification, all-application coverage,
enforced immutable system storage, multi-disk root selection or an in-place
updater. External `fsck.exfat` was not run. The minimal `/init` archive is specific
to this bounded proof; it does not claim to preserve every ordinary live-image
resource/default. No host disk was formatted, mounted or installed to, and no
host reboot occurred. All task-owned VMs are stopped; the three original review
VMs remain live and untouched.

Published in [PR311](https://github.com/PaulMDemers/SrvrOS/pull/311).
A subsequent installed-root milestone must
consume the filesystem owner's stable-volume selector and the profile owner's
home/migration contract before enabling multiple candidate volumes or upgrades.
No further implementation is required for this disposable proof, and no merge
or physical-install authorization is implied by its success.

## Merge review and next bounded milestone — November 3, 2026

User authorized review and merge if ready. The original raw logs, installed disk,
tool hashes and matched artifacts were rechecked against the portable receipt;
main remained `a936baeb`. Review found two reproducibility gaps: apply trusted some
editable plan fields as their own baseline, and the harness allowed a missing
build record. The plan checksum and mandatory exact-source build validation above
close those gaps. Twelve image/preflight tests and six build-provenance tests
cover the corrected behavior; the original eleven-test acceptance above remains
historical evidence for its named revision. The refreshed VM receipt and merge
state are recorded with PR311, rather than relabeling that original run.
Clean corrected source `1ae348fd` passes the matched build, all 18 focused tests,
both installed-disk persistence boots and the damaged-volume control, with clean
guest/host allocation and unchanged protected disk regions. The
[review receipt](assets/screenshots/installed-disk-review-manifest.json) records
its hashes and retained evidence at `.local/installation-evidence/proof-5oeg8t32`.
No remaining blocking review finding applies to this disposable-image scope.

Next proposed milestone: **identity-bound installed-volume boot and recovery in
QEMU**. Attach an installed target and a decoy/source disk; select the installed
system by stable identity, independent of enumeration order. Prove normal and
reordered boots, missing/wrong/duplicate identity refusal, recovery entry without
writing any candidate disk, and continued document/preferences persistence.
Hash the decoy before/after every case. This is still disposable image work;
physical installation, in-place upgrade and authenticated multi-user sessions
remain separate milestones.

Dependencies inspected at live heads:

- [Filesystem PR308](https://github.com/PaulMDemers/SrvrOS/pull/308), `88ea5524`:
  its namespace helper does not implement installed-root selection. The filesystem
  owner must supply an explicit stable volume identifier plus format/version/role,
  unique-match validation before writable mount, and a no-fallback recovery result
  for missing/ambiguous/mismatched targets. Installation owns the boot manifest,
  boot configuration and disk fixtures; filesystem owns validation/mount attachment.
- [Profiles PR309](https://github.com/PaulMDemers/SrvrOS/pull/309), `fa9717ad`:
  preserve the v1 portable UUID independently of disk GUID and local UID/GID;
  keep destination HOME/config/data/cache binding separate from boot selection.
  The next boot-selection slice can preserve the existing home unchanged and
  does not need guest profile activation. Profile import later requires an agreed
  destination HOME/local-binding API, guest-qualified publication/recovery semantics,
  and an app/compositor adapter for the portable preference allowlist. The host
  profile fixture's directory-rename behavior is not a guest exFAT guarantee.

Both dependency PRs were open at review. Merging them alone will not provide the
missing volume selector or guest profile consumer. Profile migration/login is
not silently added to this next boot-selection checkpoint.
