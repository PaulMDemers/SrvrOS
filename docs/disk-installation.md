# Disk installation and recovery

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

Status: implementation and disposable VM proof complete; shared-code integration
remains a draft for filesystem-owner review. Physical install, in-place upgrade,
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
