# Source checkpoint and expanded USB file workload

## Reproducible baseline

The working laptop baseline is recorded locally in `78805df` (kernel thread
stack alignment) and `7032f10` (USB transport, automatic tests, exFAT corrections
and recovery). A clean checkout exposed missing Limine include paths in host
test targets; existing binaries had hidden the defect. Commit `7b7654b` restores
those paths.

A separate clean checkout of `7b7654b` built `all test-core` successfully,
including all 16 C regression groups. It used fresh source/build outputs, pinned
submodule checkouts, pinned Limine sources and the existing Zig/host-tool cache.
This verifies a clean source build with cached dependencies; it is not a test of
installing every host dependency on a new machine. All loadable kernel sections
match the hardware-tested console payload `248a8dc15fe34246`, including addresses,
lengths and content hashes. Debug paths/archive timestamps are not claimed to be
byte-for-byte reproducible.

That payload is independently preserved at
`~/.local/share/srvros/known-good/248a8dc15fe34246`. Build evidence is under
`build/review/`: `checkpoint-rebuild.log` (initial failure),
`checkpoint-rebuild-fixed.log`, `checkpoint-kernel-comparison.json` and
`checkpoint-backup.log`. The disposable clean checkout is `build/repro-7032f10`;
its final revision is `7b7654b`.

## Expanded automatic suite

New requests use `SRVROS-AUTOTEST-V2`, reports use `srvros-autotest-v2`, and staged
profiles use `ram-usb-v6-autotest-…`. The host manifest records the expected suite,
so a v1 report cannot satisfy a new run. Old reports remain collectable and old
installed profiles remain verifiable for rollback.

After consuming and flushing the one-shot request, the guest creates
`srvros-tests/<run>.files` on the selected USB volume. It refuses an existing
directory, validates the run identifier, and requires spare space for copy-on-write
replacement and report persistence. The workload exercises:

- Nested directory creation.
- File creation, overwrite and read/extend/replace append.
- Cross-directory rename with mixed-case names.
- Zero-length creation and truncation to zero.
- Growth from 97 bytes to 393,253 bytes (384 KiB + 37), crossing four clusters on
  the laptop's 128 KiB cluster volume; shrink from 8,193 bytes to 17.
- File deletion, empty-directory deletion and empty-directory rename.
- Explicit flush, unmount/remount and complete retained-file readback.

This calls the real exFAT/VFS interfaces. It does not claim to exercise the
userspace append syscall or every shell/application operation. File mutations
are confined to the fresh run directory, in addition to the normal request,
report, probe, completion marker and filesystem metadata sidecar updates.

The retained tree contains six files (`anchor.txt`, `Renamed-MixedCase.txt`,
`empty.txt`, `truncated.txt`, `shrunk.bin`, `nested/deep/large.bin`) and three
directories (`nested`, `nested/deep`, `after`). The host checks the exact names,
types, lengths and contents after reboot. Unexpected entries, missing/deleted
names that remain, wrong contents and symlinks are rejected. Successful collection
saves `storage-files.json` with each file's length and SHA-256 alongside the
report. Guest PASS alone is insufficient for host acceptance.

## Validation and physical result

- The new build passed all 16 C regression groups.
- 23 hardware/collector/repair Python tests and three bitmap tests passed.
- The actual file workload passed on independent 4 KiB and 128 KiB exFAT images,
  including refusal to reuse an existing directory without writing anything.
- The broader filesystem suite still passed all 140 injected write/flush failure
  and completed-write interruption cases.
- QEMU passed the complete one-shot GRUB → SrvrOS test → ACPI reset → default
  entry cycle. An independent parser verified all six retained files, and
  `fsck.exfat -n` reported clean. Consumed-request replay and rejected-write
  scenarios passed. The corruption/recovery/limited-repair checks also passed.
- The updated collector still validates physical v1 run `70588005b8917c28`.

Evidence: `build/review/storage-workload-build.log`,
`storage-workload-python.log`, `storage-workload-hardening.log`,
`storage-workload-qemu.log` and `storage-workload-legacy-collect.log`.

Physical run `4b74215549a371c9`, release `9fe5cf4d5d8fa841`, passed on the
MacBookAir7,2 on 2026-10-17. Its manifest identifies clean source commit
`25d2f0e12d4f28a7add9af7ebc2e8a5387f6d5df`. After the user returned to Ubuntu,
normal collection verified the report/completion checksum and all six retained
files, including the 393,253-byte pattern and exact directory layout. No repair
was needed. All guest checks passed; physical key presses remain untested by
the automatic suite. Rendering 1,000 lines took 4,919 microseconds at 1440x900,
pitch 6144, with one presentation and 5,160,988 framebuffer bytes written.

The unmounted USB volume passed `fsck.exfat -n`: clean, eight directories and
25 files, without diagnostics. It was remounted afterward. Installed payload
hashes were verified and GRUB's one-shot selection was confirmed consumed.
Current is `9fe5cf4d5d8fa841`; previous is `dfd1690ca43b2ec0`.

Evidence is in `build/hardware-tests/4b74215549a371c9/`: `report.txt`,
`completion.txt`, `result.json`, `storage-files.json`, `launch.log` and
`offline-check.log`. This request is consumed and cannot be reused.

For future cycles, preparation stages the current build and writes a fresh
request without arming GRUB or rebooting:

```sh
python3 tools/hardware_test.py prepare --uuid 37D8-C5D0
# When ready for the physical reboot cycle:
python3 tools/hardware_test.py run --run latest
# After returning to Ubuntu:
python3 tools/hardware_test.py collect --run latest
```

Keep the flash drive connected. No persistent general-purpose workspace is
enabled by this change. That is the next milestone after this validated workload;
the interrupted-write limitations in the
[filesystem hardening notes](filesystem-hardening-2026-10-17.md) still apply.
