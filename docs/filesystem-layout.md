# Filesystem layout and root contract

Updated November 3, 2026. Global activation follows the explicit namespace proof
in [PR308](https://github.com/PaulMDemers/SrvrOS/pull/308).

## Global activation milestone

Branch `device/apple-silicon/filesystem-global` builds on main `a936baeb`, proof
`88ea5524`, and full PR252 `badfeb37`. The PR252 owner supplied a read-only
lifetime/exclusion handoff and recommended full integration, not a partial
cherry-pick. Dependency merge `c0773799` preserves main's graphics syscalls
156–161 and assigns the incoming pipe-message syscall 162. The five incoming
ports changes moved to SrvrPorts commit `0f51ba9`, draft
[PR3](https://github.com/PaulMDemers/SrvrPorts/pull/3); no tracked ports tree was
reintroduced. This branch is separate from the unchanged PR308 proof branch.

Boot atomically attaches `/bin` → `/fat/bin`, `/usr` → `/fat`, `/var` →
`/fat/var`, and `/home` → `/fat/home` after the backing volume and boot log
layout exist. Missing/conflicting targets refuse activation and retain legacy
paths. `/fat` stays available; an independently mounted `/work` stays independent.
Existing `/etc` remains the initramfs tree. Profiles retains HOME/provider policy;
this change does not select a new user identity, workspace, or root filesystem.

Path-consuming syscalls copy and normalize logical paths, resolve their backing
names, and perform guards/backend operations/publication in one masked-IRQ,
scheduler I/O owner span. Monitor `ls`, `cat`, `write` and `elf` use the same admission and view; admin
mountpoint names stay raw. Eligibility in `syscall_io_safe` is unchanged. Executable
paths are prepared under ordinary admission before entering PR252's private-child
image scope. Unix socket names retain their separate registry and path contract.

Content objects, locks, dirty-size queries, mutation guards and unmount references
use canonical backing paths. Cwd and directory handles separately retain logical
paths, so `/home/..` is `/` and fchdir/openat preserve the attachment view. Root
readdir and native indexed listing include new paths; native listing retains its
short-buffer truncation contract. Active attachment targets and their ancestors
cannot be removed or renamed through filesystem syscalls. VFS retirement removes
affected bindings before node slots can be reused; mounting a replacement at the
same raw name does not reactivate aliases. Explicit activation is required.

Host validation covers the 133 real exFAT/VFS cases normally and with ASan/UBSan,
process content writeback, logical cwd/dirfd, shared alias content/locks, protected
roots, raw listing compatibility, loader/private-image admission and PR252 lifetime,
I/O-admission and checked-usercopy regressions. The guest test is
`sysabi --namespace`; `tools/namespace_smoke.py` uses two disposable disks and two
boots for `/fat`/`/work` compatibility and persistence. Matched build and guest
results are recorded below when complete.

Known bounds remain: 159-byte mutable paths, no symlinks or nonempty directory
rename, and the existing exFAT sidecar/recovery limitations. Indexed flat projected
listing rescans the raw node table and is quadratic for a complete scan; directory
root enumeration uses a direct four-binding accessor. No cache or second lock was
introduced. This milestone does not provide physical-device acceptance.

## First checkpoint history

## Target, baseline and ownership

Base: freshly fetched `origin/main` at `a936baeb` (includes HiDPI PR303).
Branch: `device/apple-silicon/filesystem-layout`, isolated ordinary checkout
`SrvrOS-filesystem`. The integration checkout and existing review VMs/disks are
not build inputs or test targets. No physical installation or reboot is authorized
by this checkpoint; Actions remain disabled.

Own VFS namespace/mount semantics and this document. Inspected fetched PR heads:
PR252 `badfeb37` owns process, descriptors and libc lifetime; PR19 `993e920a`
owns Wi-Fi; PR301 `4c9fc184` owns native display; PR306 `5028871b` plus SrvrPorts
PR2 owns SQLite packaging. Their actual file diffs were checked. This proof adds
an explicit namespace helper and tests without editing process, syscall, libc,
boot selection or those owners' documentation. Generic Makefile test wiring is
additive. There is no competing account system, installer or filesystem backend.

Required evidence: canonical resolution and rejection boundaries; one live object
through new/legacy names; write/readback, metadata and clean unmount/remount through
real exFAT/VFS; preservation of existing file contents; full matched build and
ordinary disposable VM boot. Evidence and limitations are recorded below.

## Current source facts

- `kernel/src/main.c:mount_exfat_volume` tries whole `ahci0`, otherwise `virtio0`,
  then GPT partitions in order 2, 1, 3 through 32, then initramfs `/srvros.exfat`.
  The selected filesystem mounts at `/fat`. This is discovery order, not an
  installed-root identity or authenticated boot selection policy.
- `kernel/src/workspace.c` separately selects a unique USB exFAT serial and checks
  `srvros-work/.workspace`; it mounts at `/work`. Duplicate serials are refused.
  A serial is not a security identity. The workspace must not silently become the
  OS root or replace another user's HOME.
- VFS is a flat table of stable node addresses, with initramfs directories
  synthesized at boot. It has 32,768 total slots. exFAT supports four mounts,
  8,192 files and 2,048 directories per mount, and paths of at most 159 bytes
  including the mount prefix (160-byte buffer). Capacity includes internal
  metadata and temporary replacement files; free bytes alone do not admit a write.
- exFAT imports a constrained ASCII entry subset, rejects unsupported layouts
  and case collisions, and does not implement general Unicode or arbitrary
  upstream exFAT extension preservation. VFS lookup itself is case-sensitive.
- Mode, uid/gid, inode, link count and times are SrvrOS path-keyed `.srvros/meta`
  records, replaced using `.srvros/meta.tmp`. Default ownership is root. Imported
  inode collisions are remapped in the live VFS: inode numbers are not portable
  profile IDs or guaranteed stable identifiers across installations.
- `link`, `symlink` and `readlink` are unsupported. Generated exact-allocation
  aliases are a disk-image compatibility convention, not general hard links.
  Nonempty directory rename is unsupported. Regular-file replacement has explicit
  publication/open-target handling in process/exFAT; PR252 retains that lifetime
  ownership. Do not add a second node or content cache for a namespace alias.
- The backend flushes dirty state before mutation, orders writes and flushes,
  and clears dirty state only after completion. Uncertain failures quarantine
  writes; dirty imports skip sidecar replay. This is not a journal, crash-atomic
  rename, or automatic recovery. Copy-on-write replacement needs spare clusters.
  Clean remount tests do not prove physical power-loss recovery.

See [capacity](filesystem-capacity.md), [hardening](filesystem-hardening-2026-10-17.md)
and [the storage audit](filesystem-audit-2026-10-26.md) for detailed bounds,
refusal and fault-injection evidence.

## Practical layout, staged on implemented facilities

The table defines the contract to consume, not paths globally activated by this
checkpoint. Do not install empty pseudo-filesystem directories to imply support.

| Logical path | Backing and policy | Availability / transition |
| --- | --- | --- |
| `/` | Existing initramfs bootstrap and recovery tree | Remains the current VFS root; no new root mount replaces it |
| `/bin` | Selected system volume's `bin` | Proof mapping `/bin` → `/fat/bin`; retain `/fat/bin` |
| `/usr` | Selected system volume root; currently `bin`, `lib`, `include`, `share` as packaged | Proof mapping `/usr` → `/fat`, so `/usr/bin` and `/bin` use the same objects; this compatibility view also exposes other existing volume directories |
| `/etc` | Existing initramfs configuration/defaults | Preserve existing `/etc`; do not hide it with a binding. Persistent machine configuration/overlay is a later explicit contract, not implemented here |
| `/var` | System volume `var`, including current boot logs | Proof mapping `/var` → `/fat/var` after the backing directory exists |
| `/home` | Explicitly selected home-provider directory | Proof uses `/fat/home`; the legacy workspace alternative is `/work/srvros-work`. Profiles owner defines user subdirectories and HOME/config/data/cache policy |
| `/tmp` | No general tmpfs is implemented | A disposable disk-backed scratch directory may be created by a test; automatic clearing, sticky-bit enforcement and a global `/tmp` service are not claimed |
| `/dev` | Existing terminal/device descriptor APIs | Do not invent a general devfs mount or device-node creation capability |
| `/proc` | Existing process diagnostic interfaces | No procfs tree is added; use implemented process APIs/tools |
| `/fat`, `/work` | Existing storage/mount compatibility names | Continue to work; no move, copy, rename or automatic data migration |

System binaries, libraries and packaged resources are **release-managed**;
`var` and the chosen home tree are **mutable state**. Release-managed does not yet
mean kernel-enforced immutable: `/fat` is writable and a second alias cannot
protect the same object from writes through its old path. Read-only mounts,
credential checks and upgrade staging must precede an enforced immutable-system
claim. `/etc` defaults are backed by the boot archive, not a writable overlay.

## Volume and installation interface

For this checkpoint keep the existing exFAT image format and `/fat` mount. The
installation owner can consume that unchanged kernel/image contract immediately.
A bootable installation experiment must record which disk/partition was selected
and verify it after boot; successful probing of the first volume is insufficient
evidence of stable selection when multiple candidates exist.

The required next installed-root interface is explicit selection of a block
volume with a stable identifier plus format/version and role. Resolve exactly
one match before mount; refuse ambiguity or mismatch, and enter explicit recovery
rather than falling back to writing an unrelated volume. Keep boot/recovery
payload selection separate from mutable home/workspace selection. This selector
is a specified dependency, not implemented by the namespace proof. The
installation owner owns boot configuration and image tooling; this owner owns
mount validation/namespace attachment. No new boot parameter is asserted here.

Profiles supplies the stable profile identity, home-provider choice and relative
profile content paths. Filesystem supplies the selected backing directory,
capacity/error results and eventual credentials-aware operations. Before either
owner implements enforcement, agree on process credentials (effective uid/gid,
supplementary groups), creation ownership/umask, directory search permission,
read/write/execute checks, sticky directories, descriptor rights and metadata
persistence. Current root identity and owner-mode `access()` checks do not provide
multi-user isolation. Never serialize host paths, live handles, local inode
numbers or machine credentials as portable profile identity.

## Format decision

| Option | What it solves | What SrvrOS would still need |
| --- | --- | --- |
| Keep current exFAT for this proof | Existing bootable images, host interoperability, working bounded read/write/flush and compatibility | Real permission enforcement, links, fuller rename semantics, stronger recovery, wider paths/names; sidecar remains necessary |
| Add ext2 | Native inode ownership/modes and Unix directory/link representation | Entire driver/import/mutation integration, tooling and recovery tests; choosing ext2 does not add journaling |
| Add ext4 with a deliberately bounded feature set | Native Unix metadata plus journal-based metadata recovery | Driver, feature gating, journal replay and flush ordering, boot/tooling and failure testing; journaled metadata alone does not guarantee application data consistency |

Decision: retain exFAT for the first namespace/installation experiments, without
selecting it as the final durable multi-user root. Evaluate a native Unix format
when a backend milestone can prove the missing semantics and recovery. There is
no justified new repository or format migration in this checkpoint.

Primary sources consulted: Microsoft's [exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
for allocation, entry sets and volume flags; Linux's [ext2 format documentation](https://www.kernel.org/doc/html/latest/filesystems/ext2.html)
for inode/ownership and directory representation, and [ext4 journal documentation](https://www.kernel.org/doc/html/latest/filesystems/ext4/journal.html)
for transaction/replay guarantees and their data-mode limits. These describe the
formats, not implemented SrvrOS support.

## Namespace proof interface and migration boundary

The Linux man-pages project's [pathname resolution documentation](https://man7.org/linux/man-pages/man7/path_resolution.7.html)
distinguishes traversed directory components, missing final names for creation,
dot components and trailing-slash directory requirements. The eventual public
resolver must preserve those distinctions across a mount boundary. The helper
below accepts already-canonical storage operations only; its rejection of dots
and trailing slashes is an internal precondition, not the desired application API.
Direct retrieval of the Open Group Issue 8 pathname/rename pages returned HTTP
403 during research; no new POSIX conformance claim relies on those unread pages.

`vfs_namespace` is an explicit caller-owned view with at most eight copied
source/target mappings and 160-byte path buffers. Bind to an existing directory;
refuse root replacement, occupied public names, overlapping public prefixes and
recursive/chained aliases. No duplicate VFS node, inode, mount or storage claim is
created. Resolve a canonical absolute path once, then pass the returned storage
path to the existing VFS/exFAT operation. Lookup returns the original VFS pointer.
Legacy canonical paths pass through without translation.

This low-level API deliberately rejects relative paths, `.`/`..`, duplicate or
trailing slashes, oversize input/expansion and traversal through absent/non-directory
parents. It is not a POSIX pathname parser, chroot or security sandbox. In
particular a caller cannot escape an exported subtree through dot components;
legacy paths remain accessible by design. Bindings do not cache target pointers;
unmounted targets fail lookup/resolution until present again. A view is a mapping
to a selected path, not a mount-generation pin: callers must discard/rebind it
when changing selected volumes, rather than transparently substituting a new disk.

The caller must provide filesystem exclusion across resolve and operation,
and preserve the canonical storage path in lifetime bookkeeping. The sequential host proof does not establish concurrent resolve/operation safety.
The exFAT mutation scope is internal; a supported exclusion handoff with the
process/storage owners is still needed before runtime activation. No global
aliases, syscall ABI, directory enumeration or process path normalization change
is enabled. Before activation, coordinate with PR252 to route open/exec/stat,
create/delete/rename, cwd/dirfd, getdents, metadata, statfs and unmount/open-prefix
checks through one identity contract. Pair that with logical directory enumeration
and dot-component traversal tests. Translating only `vfs_lookup` would leave
writes, cache keys and unmount checks inconsistent and is intentionally avoided.

Migration is additive: retain `/fat` and `/work`, attach tested views after explicit
volume selection, and move application defaults only after the complete process
interface passes. Do not rename existing directories or copy user data merely to
change the public namespace. Missing homes require an explicit provider decision;
do not create a replacement home on the fallback RAM image silently.

## Verification and next action

The bounded proof is complete and published as draft [PR308](https://github.com/PaulMDemers/SrvrOS/pull/308).
Implementation commit: `6686ae18fc709a07b636b97fac9e4161a2f3b9da` on base
`a936baeb`. The later evidence update changes only this document.

| Check | Result and scope |
| --- | --- |
| Real exFAT/VFS import suite | 132 modes pass: 123 existing plus nine namespace cases; normal run and ASan/UBSan using `cc -arch x86_64` on ARM64 macOS |
| Namespace proof | Paths, bounds, binding rejection, attachment lifecycle, pointer/inode identity, persistence, rename/delete, backend quarantine and legacy workspace provider pass |
| Existing data | Original 1,100-byte fixture file remains exact after mutations and remount; mode/mtime and newly written data survive clean unmount/remount |
| Image generator | Six tests pass |
| Focused Make targets | `test-vfs-namespace test-vfs-time check-docs` pass using the documented Darwin x86_64 wrapper; 824 local Markdown links at the implementation checkpoint, 825 after the evidence update |
| Matched `all` build | Pass from clean implementation commit and pinned submodules; `build/macos-build.json` records source and hashes |
| Disposable ordinary boot | Q35/TCG, VirtIO, matched kernel/initramfs/disk: desktop and first application frame at 4.31 seconds; QMP reports running and screenshot retained |
| Cleanup | Task VM stopped through its own QMP socket; original review PIDs 61610, 6233 and 85869 preserved |

Reproduce sequentially in an isolated checkout with the compiler wrapper from
[macOS guidance](macos-development.md):

```sh
CC='cc -arch x86_64' python3 tools/test_exfat_import.py
CC='cc -arch x86_64' python3 tools/test_exfat_import.py --sanitize --output build/review/filesystem-layout/final-sanitize
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-vfs-namespace test-vfs-time check-docs
sh tools/macos/build.sh -j2 all
python3 tools/macos/run.py --headless --state-dir .local/macos-vm/filesystem-layout
```

The launcher leaves its guest running; inspect its recorded PID/QMP socket and
stop only that disposable instance after readiness. Local receipts live under
`build/review/filesystem-layout/`; the Make namespace receipt is under
`build/review/vfs-namespace/`. VM manifest, serial log and screenshot remain in
`.local/macos-vm/filesystem-layout/session-20261103T224228.262439Z/`.
The initial documentation check ran before submodule checkout completed and
reported missing dependency links; the complete-checkout rerun passed.

The final sanitizer receipt names the clean implementation commit. The ordinary
132-mode run preceded that commit and used identical implementation; focused
Make checks and the matched image/VM used the committed tree. Build hashes:

- Kernel: `ac1198e5655ff216b57fab9c1f6e182476c93b214333a6739e3ebf578cad5185`
- Initramfs: `f31a586d1073314577ce4864afd3b30af19458aed048b0bc6689942c7599baff`
- exFAT template: `c972e6c49ba978df9476b0e9cfc54108a8f263fdf5b07eea1e891318c68cee7d`

The VM verifies ordinary bootability, not use of new global paths or a clean
shutdown/remount. Persistence evidence comes from the real backend's disposable
host fixture. No physical power-loss, native hardware, permission isolation,
concurrent resolve/operation or global application path activation is accepted.
No physical boot is armed and no task-owned VM is left running.

Next required platform step: agree the exclusion and canonical-storage identity
handoff with PR252, then implement complete process/namespace activation and
logical directory enumeration as a separate milestone. Installation experiments
can consume the existing `/fat` image contract immediately, with explicit
selected-volume evidence; profiles can consume the provider boundary above.
This draft is delivered for review, with no merge requested.
