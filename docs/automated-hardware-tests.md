# USB-backed hardware test cycle

New preparations now use the expanded v2
[USB file workload](usb-file-workload-2026-10-17.md), including multi-cluster
files, rename/delete, remount/readback and independent host verification of the
retained directory tree. Older v1 reports remain collectable.

The first physical run exposed an exFAT metadata interoperability bug. See the
[correction and recovery notes](exfat-interop-2026-10-17.md) before reusing the
original prepared release below. Its report was recovered with a valid CRC;
the corrected source requires a new build/release for future writable runs.

The subsequent corrected physical run `064df72e6888db2d` / `fab44e98b19ab4b3`
passed normal collection after return to Ubuntu, including persisted report CRC
and probe validation, with no repair required. Its 1,000-line render took
4,934 microseconds. See the correction notes for the retained evidence.

The follow-up [filesystem hardening](filesystem-hardening-2026-10-17.md) adds
independent file-operation and interrupted-write recovery tests. The physical
drive's separate bitmap-length discrepancy has also been corrected and checked
offline without diagnostics.

The user confirmed the laptop's USB read-only mount on 2026-10-17. The new
profile runs tests automatically, saves a report to that flash drive and requests
an ACPI reset. GRUB consumes its one-shot selection before entering SrvrOS, so
the subsequent boot uses Ubuntu's existing default.

## First laptop run (historical)

- Run: `b287ed1f63ab4073`
- Release: `3590ad5fdac9eeba`
- USB exFAT UUID: `37D8-C5D0`
- Request on flash drive: `srvros-tests/b287ed1f63ab4073.req`
- Local manifest: `build/hardware-tests/b287ed1f63ab4073/manifest.json`

This request has been consumed. Do not launch it again or use its old writer for
new writable tests. Its manifest records the dirty source checkout and hashes
of the exact boot artifacts. Working release `ae227d9b6bda1014` was independently copied to
`~/.local/share/srvros/known-good/ae227d9b6bda1014`; the earlier independent backup
`70ddde650d0fc3f8` remains available.

For a fresh run, use the subsequent-run commands below with the flash drive
connected. The launcher asks for sudo authentication, verifies the staged release and fresh USB
request, checks that the normal GRUB default is Ubuntu, installs that exact
release, arms `srvros-current` for one boot and calls `systemctl reboot -i`.
Save work before invoking it. Installation retains the current working release
as `srvros-previous`. No firmware boot-order change is made.

SrvrOS consumes and flushes the request before testing. It writes a probe, a
report and a checksum completion marker under `srvros-tests/`, flushes them,
then requests a reset. No typing or photograph is required for a successful run.
Once Ubuntu has returned and mounted the drive:

```sh
python3 tools/hardware_test.py collect --run b287ed1f63ab4073
```

Collection checks run/build/volume identity, report CRC, consistent test results
and the probe's persisted contents. Validated results are copied into the local
run directory as `report.txt`, `completion.txt` and `result.json`. A test failure
exits with status 2; missing or corrupt results exit with status 1. No Ubuntu
collection daemon is installed.

## Subsequent runs

Each cycle needs a new request, even for the same kernel:

```sh
tools/host-env.sh make -j2 all test-core
python3 tools/hardware_test.py prepare --uuid 37D8-C5D0
python3 tools/hardware_test.py run --run latest
# After returning to Ubuntu:
python3 tools/hardware_test.py collect --run latest
```

`prepare` stages the existing build and creates a unique request file; it does
not build, install, format the drive or reboot. Preparation and launch require
an unambiguous mounted USB exFAT UUID. The guest also rejects duplicate matching
volume IDs. Linux device names are not used as identities.

## Checks and limits

The guest checks boot readiness, timer progress, 16 allocated memory pages,
FPU state across nested traps/preemption, a 1,000-line mixed-case render workload,
USB keyboard enumeration, and USB file write/flush/read. Rendering time, mode,
presentation count and framebuffer bytes written are recorded. These are smoke
checks, not the complete userspace suite or a performance threshold. Physical
key presses are explicitly recorded as `NOT_TESTED`.

Normal profiles keep USB storage read-only. The automatic profile enables writes
after finding its matching volume and fresh request. The selected partition and
underlying USB transport become writable; other partition aliases remain
read-only. AHCI stays disabled by `srvros.storage=ram`.

A consumed request cannot run again: manually choosing the same profile later
stops at the monitor. A missing completion marker is never treated as success.
Request consumption is flushed first, so a failed run may leave `STARTED` without
a completed report. Prepare a new run after investigating the failure.

Reset supports the byte-wide System I/O reset register in the
[ACPI FADT](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html).
If that form is unavailable or reset fails, the monitor displays that a manual
reboot is required. The report has already been flushed at that point. There is
no independent watchdog, so a hard kernel hang can still require a manual reset.
Physical USB persistence passed the subsequent laptop cycle described above.
QEMU independently checks the automatic reset and GRUB return sequence.

## Verification

- All 16 host C regression groups passed.
- All 20 Python installer/launcher/collector/repair tests passed; the bitmap
  repair has three additional tests.
- QEMU booted the generated profile through GRUB, consumed the one-shot
  selection, ran tests, persisted and validated the USB probe and report CRC,
  reset through ACPI and reached the default-entry sentinel.
- Replaying a consumed request stopped at the monitor without modifying the USB
  disk. A read-only backing disk rejected request consumption, skipped tests and
  returned through the default entry without modifying the disk.

Reproduce with `tools/host-env.sh make hardware-test-smoke`. The harness uses
disposable images, not host block devices. Evidence is in
`build/autotest-final-build.log`, `build/review/autotest-cycle-summary.log` and
the `build/review/autotest-*-serial.log` files.
