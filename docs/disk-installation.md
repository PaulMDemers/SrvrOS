# Disk installation and recovery

## Current target and ownership

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
it refuses unknown contents or changed sources. It verifies both GPT copies,
boot artifact readback and the copied exFAT before publishing `ready.json`.
Staging UUID and generated GPT disk/partition GUIDs are distinct and recorded.
An interrupted directory is retained and cannot be reused. These protections
cover accidental misuse of disposable fixtures; they are not a privileged,
hostile-concurrency-resistant physical installer or a power-loss durability claim.

## Evidence and next action

Status: implementation and verification in progress. Build a matched image,
run refusal/bounds/interruption fixtures, then prove app save and second-boot
persistence with one disk and no live payload. Publish the exact commands,
source identities and evidence limits here with a draft PR; do not merge or
expand to physical installation as part of this checkpoint.
