# USB-backed hardware test cycle

New preparations use v3 (`ram-usb-v8-autotest-…`). In addition to the v2
[USB file workload](usb-file-workload-2026-10-17.md), they run the real userspace
shell and `ed` in a fresh `srvros-tests/<run>.shell` directory. They edit/save,
remount/reopen, restore a workspace profile and history, write a 512 KiB file,
and print 500 lines. The collector checks nine retained output files, including
exact document/history/settings bytes and a history-recall transcript. Existing
workspace documents and settings are untouched. Older v1/v2 reports remain
collectable and their installed profiles remain launchable with matching requests.

The report includes `usb_shell_workspace`, total shell workload time, input-poll
sample count/maximum gap, and maximum timer-tick gap. `physical_key_latency` is
explicitly `NOT_TESTED`: input-poll timing is not end-to-end keyboard latency.
See the [persistent shell report](persistent-shell-2026-10-17.md) for measured
QEMU key echo under load.

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

For a bounded heavier confidence run, prepare with `--stress`:

```sh
python3 tools/hardware_test.py prepare --uuid 37D8-C5D0 --stress
python3 tools/hardware_test.py run --run latest
# After the automatic return to Ubuntu:
python3 tools/hardware_test.py collect --run latest
```

This uses a v9 boot profile to run five complete v3 suites in one boot. Each
round has an independent, pre-created single-use request, file directories,
report and CRC completion marker. The filesystem is unmounted and remounted
between successful rounds, in addition to each shell workload's own remount.
The batch stops at its first failure. The existing process/open-file guards and
120-second per-script deadlines remain in force. Any failed or incomplete test
halts indefinitely on its diagnostic screen. Interrupts are disabled after the
console is presented, preventing background test processes from overwriting the
screen or continuing writes. Photograph the screen and restart the laptop
manually. Only a complete successful run returns to Ubuntu automatically.

The parent run ID selects the entire batch for launch and collection. Its five
round IDs differ by XORing the final hexadecimal digit with 0 through 4; all
share one exact build. Launch checks every request before arming the reboot.
Collection requires all five complete reports and verifies all 75 expected
storage/shell outputs after the reboot, including earlier rounds' contents. It
writes `batch-result.json` only after validating the complete batch. Individual
round manifests and reports remain available for diagnosing a partial batch.

For this laptop's current milestone, the acceptance bar is five PASS reports,
all expected files verified by Ubuntu, automatic return, a clean offline
`fsck.exfat -n` check and clear main VolumeFlags without repair. If these pass,
move on to responsiveness work and retain the finalization diagnostics. This
is a practical confidence checkpoint, not a claim that the earlier intermittent
missing completion marker has a known cause or that power-loss recovery was
tested. Physical keyboard latency remains outside this unattended workload.

The five-round runner passed all 21 host test groups and 35 Python hardware-tool
tests. `hardware_test_smoke.py --stress` completed all five rounds, independently
verified all reports and 75 output files, checked the filesystem clean (28
directories, 131 files), and returned through GRUB's default entry. Consumed
request replay caused no disk writes or reset; the original read-only failure
case returned without changing the disk. Evidence is retained under
`build/review/stress-*`. Laptop validation of the five-round profile is pending.

The first physical batch, `c63ec9cd282d3357` / `8898d0bea1c5b09a`, returned
early. Round one left a STARTED request, a probe and a partial storage tree;
no report or completion marker was present, and the other four requests were
untouched. It did not pass. The main VolumeFlags were 2 (dirty); offline
`fsck.exfat -n` found no structural errors (27 directories, 116 files). After
preserving a sparse allocated-cluster snapshot, `fsck.exfat -p` cleared the
dirty flag, verified before remount. Evidence is in
`build/hardware-tests/c63ec9cd282d3357/`. The exact write failure remains unknown.

Following this run, the user requested an indefinite halt on failure, replacing
the 15-second diagnostic window. This applies to single runs and stress batches,
including target/mount failures, request-consumption errors, failed checks,
report or completion write/flush failures, open test handles, and failed
between-round unmounts. Consumed-request replay now also halts without writes or
reboot. Normal boots without an automatic-test request still reach the monitor.
Successful test runs retain the one-shot return to Ubuntu.

The halt revision passed all 21 host test groups and the full five-round QEMU
cycle with all reports/files verified. The consumed-request case halted without
writes or reset. A forced read-only write failure remained alive with an
unchanged diagnostic log for 17 seconds after the complete HALTED line, beyond
the removed timeout, and never reached the host boot entry. Logs and retained
serial evidence use `build/review/halt-*`. This validates the failure-display
behavior; it does not resolve the laptop's underlying storage failure.

The next physical batch, `6d31782678439e1b` / `4b4c8c1f31187cca`, produced a
validated first-round report and all 15 expected files; its shell workload took
89,051,066 microseconds. Round two (`6d31782678439e1a`) halted during storage
testing. Its shell test was skipped, and no complete report survived. The photo
records command 281450, opcode 42 (WRITE(10)), failure phase `csw`, one error,
and a quarantined device. The printed status 255 is our internal sentinel for
an untrusted completion, not a status returned by the flash drive. Report saving
then failed because the volume's write path was blocked. No later rounds ran.

This locates the fault in receipt/validation of a USB command-status wrapper,
but does not distinguish a timeout, an xHCI error, a short response, or invalid
wrapper contents. The prior diagnostic collapsed these into the same phase.
The next candidate preserves the precise failure reason, byte count, expected
and received tags, raw CSW fields, and the controller's completion code, pending
state and elapsed failure time. Timeout diagnostics now clear stale completion
values before submitting a transfer. Transfer deadlines, validation rules,
quarantine and no-write-retry behavior remain unchanged.

The physical evidence, photo, first-round results and sparse filesystem snapshot
are under `build/hardware-tests/6d31782678439e1b/`. Offline checking found no
structural errors (35 directories, 152 files); main VolumeFlags were dirty (2).
After preserving the snapshot, a clean `fsck.exfat -p` cleared the flag, verified
before remount. This is an incomplete batch, not a milestone pass.

The detailed-CSW candidate passed all 21 host test groups, with injected
transport failures, short/zero-length wrappers, signature/tag/residue/status
errors and controller timeout/error-event checks. The final byte-count
adjustment was rebuilt and passed the USB storage host tests. The resulting
payload passed a complete QEMU automatic cycle, independent file/CRC checks,
consumed-request halt, and the forced write-failure halt with controller details
visible. Evidence uses `build/review/csw-diagnostics-*`. This candidate improves
diagnosis only; it does not claim to fix the physical USB fault.

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

Normal console profiles keep USB storage read-only. Workspace profiles opt in
through their separate UUID/marker check. The automatic profile enables writes
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
