# Persistent shell environment and unattended tests

## User workflow

After booting a prepared workspace profile, enter `workspace shell` at the
monitor. The shell starts in `/work/srvros-work`, sets `HOME` to that directory
and `HISTFILE` to `.history`, then reads `.profile`. The host preparation helper
creates a starter `.profile` with `HISTSIZE=128` only if the file is absent;
existing settings and documents are preserved. The equivalent shell invocation
is `sh --workspace /work/srvros-work`. Invalid paths and a failing profile return
an error instead of opening an unexpected environment.

Workspace history stays in memory while commands run. `exit` and Ctrl-D save
and flush it; use `history -w` and `sync` to save earlier. This avoids full USB
history rewrites between ordinary commands. Forced termination can lose history
since its last save. Document writes are still handled by the application;
history batching does not delay their writes. Ordinary shells keep their existing
history behavior. Child scripts inherit the working directory, but do not
implicitly execute `.profile` again without `--workspace`.

To edit a saved document with the existing line editor:

```sh
ed -s notes.txt
```

Example editor input:

```text
a
A line of persistent text.
.
w
q
```

Then run `sync`, `exit`, and `reboot` at the monitor. `ed` remains a small line
editor, not a full POSIX editor implementation. It now reports command and
write/close failures through a nonzero exit status; an unreadable existing input
file also fails. A missing filename can still start a new document. History
load/save now checks stdio errors and close failures, and workspace exit reports
failed history saves or flushes.

## Automatic hardware suite v3

`hardware_test.py prepare` now creates `SRVROS-AUTOTEST-V3` requests, with
`srvros-autotest-v3` reports and `ram-usb-v8-autotest-…` boot profiles. The normal
workflow remains `prepare`, `run`, then `collect` after Ubuntu returns. No manual
shell commands or photos are needed for this test. Older v1/v2 report collection
and launches of their matching installed payloads remain supported.

After the existing memory/FPU/render/storage checks, the kernel creates a fresh
per-run `.shell` directory, writes controlled scripts/input fixtures, and launches
the actual `/fat/bin/sh --workspace <directory>`. The workload:

- Loads persistent settings and records their value.
- Runs `ed` to substitute and append lines, then saves the document.
- Opens an interactive shell with recorded stdin to save real history.
- Writes a 512 KiB file and prints 500 terminal lines.
- Flushes and unmounts/remounts the USB filesystem.
- Reopens the document with `ed`, compares its contents, and opens a second shell
  to recall history and settings.
- Checks process exit status and exact contents before recording PASS.

Each shell process has a 120-second cooperative deadline. Timeout requests
termination of its process group. Remaining open files prevent automatic reset;
a stuck kernel/driver still requires separate watchdog support. The host collector
independently validates the edited/reopened documents, history, settings, markers,
zero-filled data and the history-recall transcript after reboot. A complete v2
report cannot satisfy a v3 request, nor can a v3 report omit the new check.

## Responsiveness measurements

The guest report records total shell workload microseconds, input-poll sample
count/maximum gap, and maximum timer-tick gap. These are service diagnostics;
physical key latency remains explicitly untested by the unattended suite.

`tools/workspace_latency_smoke.py` injects USB keys through QEMU and measures host
monotonic time until the character is echoed to serial. The disposable USB disk
is deliberately throttled to 200 write IOPS for one phase. A sample run measured:

| Condition | Samples | Observed echo latency |
| --- | ---: | ---: |
| Idle | 10 | 4.4–14.0 ms |
| Active throttled USB write | 1 | 5,093.7 ms |
| Active 20,000-line terminal output | 1 | 24.3 ms |

All injected keys were retained, the 512 KiB file matched exactly, and independent
exFAT checking reported clean. These sparse, host-clock QEMU samples are neither
laptop display latency nor performance acceptance thresholds. QEMU was running
alongside other regression work, so host scheduling also affects them.

The slow-write result exposes the next performance issue: filesystem mutation
ownership allows I/O service work to run, but excludes ordinary application
threads until the mutation completes. Keyboard reports can be collected while
the shell waits to echo them. Further work needs finer filesystem synchronization
so applications can run safely between transfers. The internal poll-gap counter
alone cannot establish interactive responsiveness.

## Validation and evidence

Commands:

```sh
tools/host-env.sh make -j2 all test-core
PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_hardware*.py'
tools/host-env.sh python3 tools/workspace_smoke.py
tools/host-env.sh python3 tools/hardware_test_smoke.py
tools/host-env.sh python3 tools/workspace_latency_smoke.py
tools/host-env.sh python3 tools/cli_smoke.py --boot-wait 60 --shell-wait 10
```

The focused tests cover workspace path/profile guards, history deferred until
exit and restored on the next boot, editing/reopening, and existing readonly/
failed-write behavior. Host I/O tests exercise actual editor/history code with
failed buffered writes/closes. QEMU automatic tests cover a successful run,
consumed-request replay without writes/reset, and a read-only device failure.
The build and all 20 host test groups passed, as did 34 Python hardware-tool
tests, all 481 CLI smoke commands, and the focused workspace, automatic-test,
and USB-key responsiveness runs. The CLI run preceded the final editor
unreadable-input guard; host editor tests and both focused workspace/automatic
QEMU runs subsequently passed against that final payload.
Evidence is in `build/review/workspace-shell-*`, `workspace-latency.json`, and the
`autotest-*` report/serial files. Physical validation of this revision is pending;
the previous workspace persistence release remains available for rollback.

## Laptop timeout and redirected-output correction

Physical run `d68f1e4b34d95ab3`, release `dbc08f2aca95a03c`, reported FAIL.
The existing storage tests passed, but the second shell phase exceeded its
120-second deadline (235,049,773 microseconds across the complete shell workload).
The open-file guard correctly kept the machine at the monitor instead of
rebooting with live handles. The photo shows a process exiting afterward; this
was not a successful automatic return.

Ubuntu collected the CRC-validated failure report and preserved a complete copy
of the partial shell directory in `build/hardware-tests/d68f1e4b34d95ab3/`.
The document, reopened document, settings, profile, complete four-line history,
first-phase marker and 512 KiB payload all matched. The transcript reached
`exit`; the final verification marker and history-proof file were absent.
Offline `fsck.exfat -n` reported clean, with 14 directories and 57 files.

The redirected-output path called `process_file_flush` after every write syscall.
Unlike ordinary file-descriptor writes, even a single echoed character caused
an exFAT replacement-file transaction. The recorded interactive history session
therefore repeatedly rewrote its growing USB transcript. This is a concrete
source of avoidable I/O consistent with the laptop timeout.

Redirected stdin/stdout/stderr file writes now use the existing buffered file
path. Process exit flushes redirected write handles before publishing its status,
including handles still shared with a waiting parent, and reports flush/close
failure as exit status 1 when the requested status was 0. Existing nonzero exit
statuses are retained. Explicit `fsync` resolves redirected descriptors 0–2;
`sync` and final close also retain their existing flush behavior. Pipes continue
to deliver immediately and console output is unchanged. The 120-second test
deadline is unchanged.

An independently opened file can see the last flushed contents while its producer
is still running: use `fsync`, `sync` or process completion before readback.
This does not add coherent live readers or change the existing 16 MiB write-buffer
limit. It also does not remove the filesystem mutation ownership restriction
identified by the slow-disk latency test.

The new host regression exercises the actual process I/O code: 1,000 one-byte
redirected writes produce no filesystem transaction until flush/exit, then one
exact write. It covers a parent-held duplicate, explicit flush, stderr aliasing,
writable stdin, immediate pipe delivery, and injected save failures. Physical
confirmation of the correction requires a fresh one-shot run.

The workspace regression also caught a separate serial RX race: `ed -s` was
received as `ed- s`. Polling could read a UART byte, be interrupted by the RX
handler publishing the next byte, then publish the original byte afterward.
Polled FIFO draining and ring consumption now share an interrupt-masked section;
the host test checks FIFO order and preservation of the caller's interrupt state.
This concerns serial input, not USB keyboard report decoding. The failing
transcript is preserved as `build/review/output-batching-serial-race.log`.

The corrected payload passed all 21 host test groups, the workspace persistence
smoke test and the complete QEMU automatic boot/test/return cycle. The shell
workload in that QEMU run took 20,534,682 microseconds; this is an emulator
measurement, not an estimate for the laptop. New validation logs use the
`build/review/output-batching-*` prefix.
All 481 CLI smoke commands also passed with output batching. That run started
before the additional serial RX correction; the final-payload host, workspace
and automatic-cycle runs cover the subsequent RX change.
