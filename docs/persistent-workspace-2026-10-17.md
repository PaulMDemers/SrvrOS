# Persistent USB workspace

The optional workspace profile keeps the boot filesystem at `/fat` in RAM and
mounts one explicitly selected exFAT USB volume at `/work`. Documents and shell
scripts go in `/work/srvros-work`. The whole selected volume becomes writable;
this directory is a convention, not a sandbox. Internal SATA remains disabled
by the RAM-root hardware profile. Other USB volumes remain read-only.

## Prepare and install from Ubuntu

With the existing USB volume mounted:

```sh
tools/host-env.sh make -j2 all test-core
python3 tools/hardware_workspace.py --uuid 37D8-C5D0
sudo python3 tools/hardware_boot.py install --release <printed-release>
```

The preparation tool checks USB ancestry and a unique exFAT UUID, creates
`srvros-work/.workspace`, and adds `README.txt` and `welcome.sh` only if absent.
It preserves existing user files and refuses a conflicting marker or symlink.
The marker contains exactly `SRVROS-WORKSPACE-V1 37d8c5d0` followed by a newline.
Staging verifies build inputs before changing the USB directory. Metadata is
saved in `build/workspace/manifest.json`.

The installed profile is `ram-usb-v7-workspace-37d8c5d0`. Installing does not
reboot or select the next GRUB entry. When ready for the interactive test:

```sh
sudo grub-reboot srvros-current
sudo systemctl reboot -i
```

This is a persistent interactive workspace profile, not an automatic hardware
suite. The UUID and marker enable it on each deliberate boot of this profile.
The GRUB selection itself is one-shot, leaving Ubuntu as the default.

## Use in SrvrOS

At the monitor:

```text
workspace
run /fat/bin/sh
```

In the shell:

```sh
cd /work/srvros-work
sh welcome.sh
echo MixedCase-document > notes.txt
echo second-line >> notes.txt
cat notes.txt
echo 'echo persistent-script-ok' > saved.sh
sh saved.sh
sync
exit
```

Back at the monitor:

```text
workspace sync
workspace unmount
reboot
```

`reboot` also flushes and unmounts an active workspace automatically. It refuses
if files are still open or flushing fails. Successful `workspace unmount` (or
`unmount /work`) prints `workspace: unmounted; USB writes disabled`. At that
point the drive can be removed. `workspace mount 37D8-C5D0` reopens a prepared,
still-connected volume; USB unplug/replug discovery is not implemented.

On return to Ubuntu, the files are under `srvros-work` on the same USB volume.
Booting the workspace profile again should retain both documents and scripts.
A normal console profile can explicitly opt in with `workspace mount UUID`.

## Failure handling and limits

A missing/duplicate UUID, wrong marker, already writable device, existing mount,
or dirty/unsupported exFAT volume prevents activation. Mount/unmount operations
retain the scheduler I/O owner so user tasks cannot open files between the busy
check and removal of the mount. Successful unmount revokes write callbacks from
the raw USB device and every partition alias.

A workspace flush failure latches a failed state and leaves the workspace
mounted. A rejected unmount is not permission to unplug. Check the volume from
Ubuntu before attempting further writes. The shell's `sync` now returns failure
when the sync syscall fails, so `sync && echo saved` cannot hide that failure.
This does not retroactively report every earlier application write error;
applications must still check their own writes and closes.

Existing filesystem limits remain: ASCII names, case-sensitive VFS lookup,
one-cluster allocation bitmap support, and no journal or guarantee against a
torn sector. This feature does not add networking, GUI work, or general hotplug.

## Validation

Run the focused integration suite with:

```sh
tools/host-env.sh make workspace-smoke
PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_hardware*.py'
```

The QEMU suite uses disposable, independently formatted 128 MiB exFAT media with
128 KiB clusters. It checks default read-only behavior and missing-marker refusal
without disk changes; shell creation, append, script execution and explicit sync;
unmount/revocation and remount; reboot and exact retained-file bytes on the second
boot; and refusal to unmount or reboot after a failed write. Independent
`fsck.exfat -n` must report clean without `ERROR`, even if its exit status is zero.

The workspace test also exposed and fixed a pre-existing shell bug: child
shells discarded the inherited `PWD` and always started at `/`. Shell startup
now accepts a bounded, absolute inherited path only when it names a directory;
otherwise it falls back to `/`. Both relative script execution and `sh -c` file
access are exercised from the workspace, with relative scripts added to the CLI
regression suite.

Host tests cover duplicate UUIDs, invalid markers, dirty volumes, active I/O
ownership, open-file refusal, failed flush/buffered sync, all-alias revocation,
preparation preserving user files, profile conflicts, and the actual shell sync
command's failure status. The existing automatic hardware-test QEMU cycle is also
rerun to check that workspace support leaves those profiles working.

Evidence is stored under `build/review/workspace-*`. Physical validation of this
workspace profile is pending; the prior USB workload and reset path have already
passed on the MacBook Air. The verified prior releases `9fe5cf4d5d8fa841` and
`dfd1690ca43b2ec0` are retained independently under
`~/.local/share/srvros/known-good/`, alongside the normal console rollback build.

The development validation completed successfully: `all test-core` (18 C groups),
31 hardware/profile/preparation Python tests, all five workspace QEMU scenarios,
and the 481-command CLI suite. The final CLI filesystem check reported zero
leaks, FAT mismatches or errors. The automatic hardware-test regression also
passed its successful cycle, consumed-request replay and write-failure scenarios.
