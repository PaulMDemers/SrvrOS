# Filesystem capacity and allocation

The filesystem scale candidate replaces the old static 1,024-file / 256-directory
exFAT tables and 4,096-node VFS table. Native acceptance is tracked separately in
the [Air7,2 record](machines/macbookair7-2.md); an installed older release retains
its older limits.

| Resource | Budget | Allocation |
| --- | ---: | --- |
| exFAT files per volume | 8,192 | Stable blocks of 64 file records and paths |
| exFAT directories per volume | 2,048, including mount root | Stable blocks of 64 directory records |
| Mounted exFAT volumes | 4 | Independent tables and metadata scratch |
| VFS nodes across all filesystems | 32,768 | Bootstrap block of 256, then stable blocks of 256 |
| exFAT path | 159 bytes plus terminator | Unchanged |

These are admission budgets, not preallocated arrays or a promise that every
combination fits available RAM. `vfs_free_nodes()` reports unused budget;
`vfs_reserve_nodes()` attempts the corresponding memory allocation. Before disk
mutation, exFAT reserves file/directory blocks, VFS nodes, metadata replacement
nodes and serialization scratch. An allocation refusal at that stage performs
no disk writes. Later I/O/buffer failures retain the existing dirty/quarantine
and recovery policy; this is not a transactional filesystem.

Pointers to live VFS nodes, exFAT private file records and stored paths stay
stable across growth. Deleted file and directory slots can be reused, including
holes below the high-water mark. Unmount unregisters VFS nodes before freeing
exFAT blocks and scratch. VFS blocks remain allocated until VFS initialization,
because callers can inspect a retired node's `used` flag; repeated mount/unmount
reuses that bounded high-water allocation. Kernel heap initialization precedes
VFS imports so larger boot archives can allocate nodes safely.

The VFS keeps a sorted dense list of live slot IDs. Indexed enumeration is
constant-time and preserves slot order; prefix removal compacts it once. Path
and inode lookup remain linear. exFAT still rewrites the complete metadata sidecar
on each ordinary mutation. The scratch buffer grows on demand to a worst-case
bound derived from the node/path budgets, and cannot move while a nested sidecar
write references it. The sidecar and exFAT disk formats are unchanged.

Use [workspace admission and verified archival](ubuntu-device-agent-setup.md#workspace-log-capacity-and-verified-archival)
even with the larger budget. Retention never deletes unknown files automatically.
Linux preflight reads limits from the checkout: validate and install the matching
payload before relying on increased capacity on physical media.

## Validation

Run these sequentially in one checkout:

```sh
tools/host-env.sh make test-core test-exfat-hardening test-exfat-scale
tools/host-env.sh python3 tools/hardware_test_smoke.py --release RELEASE --scale-files 5000
```

`RELEASE` is a staged v8/v9 automated-test payload. The VM command uses disposable
media, exercises save/remount/reopen and automatic return, checks preserved files,
and retains the consumed-request and read-only-failure negative cases. It does
not install or reboot the host.

The host scale fixture imports 5,000 files, grows/reuses 96 files and 320
directories, verifies retained references and identities, edits/renames/remounts,
and checks exact contents with an independent parser and `fsck.exfat`. Allocation
faults cover mount rollback/retry and mutation refusal before writes. The hardening
suite additionally reaches both budgets, serializes a large metadata sidecar,
reuses directory holes at exhaustion, and injects write/flush/power-cut failures.
