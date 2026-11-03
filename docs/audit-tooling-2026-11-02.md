# Tooling audit — 2026-11-02

Scope: the 561 paths under `tools/` tracked at `dc0a3fef`, read from the current shared checkout, which also contained concurrent changes. The host build/install and hardware boot paths were separately assigned; their combined follow-ups are recorded below. This is a risk-focused audit with whole-tree screening, not a line-by-line proof of every fixture or script.

## Coverage and method

`git ls-tree -r --name-only dc0a3fef tools` identified 295 Python, 232 C, 4 C headers, 3 shell and 6 ASL sources, plus 21 text, marker and binary/image fixture files. All 561 tracked paths were opened. The 540 source files contained approximately 73,731 lines in the working snapshot. A Python AST parse **and compile** covered all 295 Python sources without syntax errors; lexical scans covered all C and headers for assertion and potentially unsafe library-call patterns, and `sh -n` checked all three shell scripts. `file` classified the 21 non-source files; this was an inventory, not semantic validation of all image and USB fixtures. The initial screen treated ASL as text; all six sources were subsequently compiled as recorded below.

The whole-tree screens counted 1,970 Python `assert` nodes in 106 files, 115 broad exception handlers, 454 subprocess calls, and approximately 10,360 C `assert` tokens. Searches covered archive extraction, subprocess status handling, unchecked/ignored decoding, dynamic evaluation, C string and file I/O, PASS markers and parser return paths. These counts are screening signals, not findings by themselves. Manual tracing focused on evidence/receipt consumers (`battery_fault_records`, `desktop_power_records`, `hardware_cpu_load`, `hardware_desktop`, `serial_records`), symbol audit behavior, exFAT host fixture assertion use, archive pin/extraction, and cleanup exception handlers. The assert/import reachability estimate below is static and may overstate protection for conditional imports; it is not a proof for dynamic imports.

## Findings and changes

1. **Evidence could pass with checks removed by Python optimization.** Before the change, `PYTHONPATH=tools python3 -O -c 'from battery_fault_records import check_fault; print(check_fault(b""))'` printed `True`: the empty log reached a success return because `-O` removed the validation assertions. Twelve assertion-based `*_records.py` validators now reject optimized interpreters at import. The two shared admission modules `serial_records` and `desktop_foundation_smoke` and eight otherwise unguarded smoke/timing entry points also fail closed. Static import-graph screening reaches 92 of 94 non-test Python files containing assertions; the remaining two (`audio_fixture.py` and `battery_filesystem_image.py`) are fixture generators. Guard tests launch an actual optimized interpreter for each directly guarded module. The reproduction now exits nonzero before parsing. Ordinary, non-optimized Python remains required for assertion-based developer fixtures and tests. C fixtures likewise require assertions enabled: several put essential fixture operations inside `assert`, so `-DNDEBUG` is not a supported way to run them.

2. **The libc symbol audit could certify an empty audit in normal Python.** With an existing header containing only a comment and a successful `nm` emitting no symbols, `libc_symbol_audit.py --fail-missing` exited 0 and printed zero declarations and zero missing exports. It now refuses an empty declaration set and reads headers as strict UTF-8 instead of silently discarding undecodable bytes. Subprocess tests cover empty and undecodable headers, a missing export, and a present export. This is not a complete C declaration parser: unsupported syntax in a nonempty header set can still be omitted, and a successful audit should not be interpreted as proof that every declaration was understood.

## Verification and limits

- `python3 tools/test_optimized_records.py`: passed, exercising each guarded module with `python -O`.
- `PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_*records.py'`: 105 tests passed after changes, including optimized-import coverage.
- `PYTHONPATH=tools python3 -m unittest test_hardware_desktop test_hardware_cpu_load test_serial_records test_libc_symbol_audit test_optimized_records`: 58 tests passed in the earlier run; the optimized-module list was subsequently expanded and rerun separately.
- `python3 tools/test_libc_symbol_audit.py`: two tests passed, including actual CLI subprocesses and a fake `nm`.
- `git diff --check` passed. No shared Makefile, shared build, or VM was run for this audit. The full 73-file Python test inventory and the 232 C programs were not executed: some require external host utilities, generated drivers, VM or hardware. Their whole-tree coverage is the static screening described above.

Optimized-mode guards deliberately fail closed; they do not make `assert` reliable under `-O`. Shared admission guards protect import paths actually traversed, while two fixture generators, developer tests, other languages and future scripts still require the documented normal interpreter/assert-enabled build. PASS substring consumers in smoke harnesses and fixture semantics remain candidates for deeper evidence-specific negative testing; static scanning cannot establish whether every combination of malformed, duplicated, stale, or truncated output is rejected. The guard and symbol-audit changes do not establish hardware acceptance or validate any real boot, disk, or device.

## Linux checker-version follow-up

A disposable malformed exFAT image with a bitmap length one byte larger than
`ceil(cluster_count / 8)` was independently inspected from its directory entry.
The Linux audit container's exfatprogs 1.2.9 `fsck.exfat -n` returned success and
reported it clean; other versions have emitted an `invalid bitmap size`
diagnostic. Therefore a clean fsck result does not prove this exact property.
`test_exfat_bitmap.py` now checks the entry and geometry independently, verifies
the repair restores the entire original image byte-for-byte, and explicitly
reports when the external checker does not detect it. A separate mocked check
verifies the wrapper rejects an error diagnostic even with exit status zero.
The five Linux fixture tests pass; no physical volume was opened or changed.

## Boot payload and image generation follow-up

The installer previously accepted a selected release directory whose internally
valid manifest named another release. It also copied unlisted entries that were
absent from its manifest and space accounting. Verification now requires the
selected identity and exact regular-file layout, including the loader directory.
Staging publishes a fully verified directory by rename and updates `latest`
atomically. Installation verifies the retained and obsolete payloads before
changing the installed configuration, then rechecks an obsolete payload before
removing it. The 88-case private installer suite passes. Negative controls
reproduce identity and preflight-order failures against the previous behavior.

FAT sizing oscillated forever for 507 MiB and 1020 MiB ESP sizes. Binary search
now selects the smallest FAT that holds every data-cluster entry. Default
384 MiB geometry stays unchanged. The image builder also accepts a basename-only
output path. The three image tests cover default geometry, both oscillating
sizes with subprocess deadlines, and a real disposable image with MBR/GPT checks.
The latter fails against the old basename handling.

All six tracked ASL fixtures compile with the Linux container's iasl. Additional
Python workload/evidence suites that were absent from `test-core` are now wired
into the existing host-tools target. The device-check replay fixture snapshots
nested cache directories as well as files. New syscall host fixtures explicitly
select the host IRQ shim, and the TCP accept fixture initializes its local MAC
as production `net_init` does. These correct the test environment without
removing production validation.


The final isolated Linux GCC 14 `make test-core test-acpi-namespace` run passes
all 173 direct core targets and the additional 30-case offline namespace suite.
Core reports 385 Python cases in 53 groups; with the namespace suite the combined
run reports 415 cases in 54 groups. The expanded user-copy fixture is included.
The final log is `build/review/whole/linux-core-173-final.log`. A preceding
172-target attempt exposed a duplicate logging symbol in the GAS host fixture;
using uACPI's formatted-logging hook corrected the fixture before this full pass.
