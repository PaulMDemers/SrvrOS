# exFAT interoperability correction

The first laptop automatic run, `b287ed1f63ab4073` / release
`3590ad5fdac9eeba`, persisted all four test files but Ubuntu could not open three
of them. Directory enumeration showed their names while `stat` returned ENOENT.
The collector initially described this as a missing report, which was misleading.

A read-only raw-device recovery verified the report/completion CRC and probe
contents. The report records PASS for boot readiness, timer, memory, FPU traps,
rendering, USB keyboard enumeration and the storage probe. The 1,000-line render
took 4,939 microseconds at 1440x900, pitch 6144, with one presentation and
5,160,988 framebuffer bytes written. Physical key presses were not tested. The
user does not recall needing a manual reset; automatic laptop reset remains
unconfirmed.

## Cause and correction

The filesystem writer omitted `NameHash` in newly created/renamed entry sets and
omitted the mandatory `AllocationPossible` flag. Overwriting an existing file
updated its stream extension without refreshing the encompassing entry checksum.
The captured request has a stale checksum; the new probe/report/completion and
`.srvros` metadata records have zero name hashes and missing allocation flags.

The writer now hashes the mandatory ASCII upper-case mapping as UTF-16LE, sets
the allocation flag and refreshes the complete entry-set checksum when changing
length or cluster information. It rejects unsupported non-ASCII names instead
of emitting invalid byte-to-UTF-16 conversions. Packed name records are read
through byte pointers. Existing filename bytes are preserved on a resize.

The [Microsoft exFAT specification](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
defines these fields in sections 6.3.3, 7.6.2 and 7.6.4, and the mandatory ASCII
mapping in 7.2.5. Linux uses the filename hash for lookup, explaining why names
appeared in a listing but could not be opened.

## Recovery and limited repair

`tools/inspect_hardware_test.py --run ID` opens the UUID-selected device read-only,
reads bounded directories and known test/metadata files, and writes JSON to
stdout. It verifies the recovered report and probe separately from filesystem
validity. The original evidence is
`build/hardware-tests/b287ed1f63ab4073/recovery.json`.

`tools/repair_hardware_test.py --run ID` prints a plan from that evidence without
opening the device. `--apply` requires the USB volume to be unmounted, opens it
exclusively, verifies the current volume, layout, contents and original records,
and durably saves `repair-plan.json` before writing. It changes only flags,
name hashes and checksums in the four run records plus `.srvros` and `.srvros/meta`.
It does not change file contents, cluster allocations, lengths or unrelated
directory entries. Unexpected changes cause refusal; already-applied records
are accepted so an interrupted repair can be completed.

For this run, after closing applications using the flash drive:

```sh
udisksctl unmount -b /dev/disk/by-uuid/37D8-C5D0 && \
sudo python3 tools/repair_hardware_test.py --run b287ed1f63ab4073 --apply && \
udisksctl mount -b /dev/disk/by-uuid/37D8-C5D0 && \
python3 tools/hardware_test.py collect --run b287ed1f63ab4073
```

The collector now distinguishes missing results from listed entries that Linux
cannot open. It still refuses to accept inaccessible or incomplete results.
Do not reuse the original boot payload for another writable test; build/stage
the corrected source before preparing a fresh run.

Corrected normal console release `ff6be76bcdfff11c` is staged, with kernel SHA256
`d28c269c3b393e38be409671d4a394f5becfec52ccda3c6af9fc8057f5017a29`.
It was installed and its EFI payload hashes verified after the physical repair.
Future automated profiles use a new run ID and therefore a different release ID
for the same kernel.

## Verification approach

The original QEMU test used our own image builder and reader, which tolerated
these omissions. That was insufficient interoperability coverage. The automatic
test harness now uses Ubuntu's `mkfs.exfat` with 128 KiB clusters, matching this
flash drive, and runs `fsck.exfat -n` both before and after guest writes. It also
checks every recovered record's hash, allocation flag and checksum. The checker
does not itself reject a missing allocation flag, so that assertion is explicit.

Fault injection corrupts filename hashes and entry checksums in disposable copies:
the independent checker must reject both. Read-only recovery must still verify
their report payloads, and the limited repair must restore the exact original
image bytes and pass the checker. Replay and rejected-write tests continue to
exercise the one-shot boot cycle.

The C regression exercises the actual entry writer and resize path. Python tests
cover repair scope, payload/identity rejection, unchanged surrounding bytes,
replay after partial completion, refusal of changed entries, and the collector's
inaccessible-file diagnostic. Logs are `build/review/exfat-interop-cycle.log`,
`build/review/autotest-invalid-*-fsck.log`, `build/exfat-interop-build.log` and
`build/review/exfat-host-python.log`.

The corrected build passed all 16 C regression groups, 20 Python tests, the full
three-boot QEMU cycle and both independent corruption/recovery/repair checks.
All 479 shell/filesystem regression commands also passed, including the POSIX
probe and final guest fsck (`build/review/exfat-interop-cli.log`).

## Physical repair result

The six-record repair was applied on 2026-10-17 with the USB volume unmounted.
Original bytes and replacements are saved in the run's `repair-plan.json`.
After remounting, Ubuntu can stat/read the report, completion marker and probe;
`hardware_test.py collect` validates the report CRC, identities and probe and
reports PASS. Normal collected files are `report.txt`, `completion.txt` and
`result.json` in `build/hardware-tests/b287ed1f63ab4073/`.

The offline `fsck.exfat -n` run exited 0 and reported four directories and seven
files, but also flagged the allocation bitmap's existing length of 59,150 bytes
(the Linux mount log already reported a required length of 59,149 before repair).
That separate discrepancy was not changed by the targeted directory repair;
this is not a claim that the checker produced no diagnostics. No format or
allocation change was performed.

The corrected console release is installed as `srvros-current`; no reboot was
requested or one-shot GRUB entry armed. Installation output is preserved in
`build/review/exfat-fixed-install.log`. The independently verified working backup
`~/.local/share/srvros/known-good/ae227d9b6bda1014` remains available.

## Fresh physical run with corrected writer

Run `064df72e6888db2d`, release `fab44e98b19ab4b3`, was launched through the
one-shot GRUB workflow. After the user returned to Ubuntu, the normal collector
read the new files directly and validated the report CRC, build/run/volume
identity and persisted probe. No raw recovery or directory repair was needed.
Every guest check reports PASS; physical key presses remain explicitly untested.
The 1,000-line rendering workload took 4,934 microseconds, with one presentation
and 5,160,988 framebuffer bytes written at 1440x900 (pitch 6144).

Results are in `build/hardware-tests/064df72e6888db2d/report.txt`, `result.json`
and `completion.txt`. The installed current entry is this consumed test profile;
the previous entry is corrected normal console release `ff6be76bcdfff11c`.
Another automated test requires a fresh request; selecting the consumed profile
stops at the monitor. The separate allocation-bitmap length discrepancy was
subsequently corrected, with an offline check producing no diagnostics. See the
[filesystem hardening follow-up](filesystem-hardening-2026-10-17.md) for the
backup, expanded tests, and replacement build.
