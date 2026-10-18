# Desktop input during USB filesystem activity

This opt-in trial follows the accepted [interactive scrollbar checkpoint](trackpad-scroll-2026-10-17.md#interactive-scrollbar-physical-result--2026-10-17).
Physical acceptance is pending. Existing accepted images remain independently
backed up; the normal desktop and earlier pointer/gesture profiles are unchanged.

## Workload and evidence

`deskio` is a separate userspace process. It requires the matching consumed
trackpad request and exclusively creates `/work/srvros-work/desktop-<run>`.
An existing directory or temporary file causes refusal. Each round writes a
256 KiB run/round-specific pattern in 32 KiB chunks, fsyncs/closes it, removes
the previous test payload, renames the temporary file, reopens and verifies every
byte and EOF, and syncs storage. It retains the last `payload.bin`. The workload
is paced between rounds, is bounded to 128 rounds (32 MiB logical payload), and
can stop gracefully after its current round. Actual device traffic can be higher
than logical bytes because of filesystem metadata and write behavior.

The guest read check uses the VFS and can be cache-backed. It is not independent
proof of media contents. Ubuntu collection independently compares the complete
persisted final payload with its expected pattern and records its SHA-256.
The disposable QEMU image also receives an offline exFAT check.

Progress travels through a pipe. The compositor reads it without blocking and
shows RUNNING/COMPLETE/STOPPED/FAILED plus rounds and input counts. File operations
stay in the worker, apart from the compositor's final small evidence record after
the worker stops. Escape requests graceful stop; the desktop waits for worker
exit before returning to capture/save checks. An unresponsive stop has a bounded
failure path and cannot produce an accepted result. SIGPIPE is ignored for the
stop channel because the worker can finish just before Escape.

The worker polls redirected stdin for stop readiness. Bring-up exposed that
libc's existing `fcntl` handling for descriptors 0–2 accepts but ignores
`O_NONBLOCK`; an attempted nonblocking read therefore stalled before round one.
This trial avoids that path. Correct redirected-stdio flag semantics remain
separate library work rather than being silently claimed fixed here.

Checksummed `result.txt` and `ui.txt` identify the run, worker state, verified
rounds, logical bytes and input counts. The collector requires these alongside
the existing identity/CRC/completion-marker protected trackpad report and the
observed host reboot. Files and payload are archived in the host run directory.

The combined coverage gate requires at least 16 verified rounds, eight delivered
text characters, pointer motion, wheel events, two user focus changes and a
captured drag cancellation while the worker reports RUNNING. These counters
describe activity during a paced workload; they do not prove every input event
overlapped a USB transaction, nor measure input-to-display latency. User feedback
is still needed for stalls, missing input and correct drag cancellation.

## On-screen workflow

1. While RUNNING, type, scroll with two fingers, drag the thumb and resize.
2. Ctrl-N opens a second study. Tab switches focus; type in both windows.
3. Hold the scrollbar thumb, press Tab, then release the pad. Moving afterward
   must not continue the old drag. Check that a fresh drag works.
4. Repeat until at least 16 rounds complete; note stalls or missing input.
5. Escape finishes file work, then displays the existing live filter/save checks.
   Clear trial typing with Escape, enter `AaZz09` and press Enter. The machine
   saves, unmounts the USB drive and halts indefinitely for manual restart.

```sh
python3 tools/hardware_trackpad.py prepare --uuid 37D8-C5D0 --desktop-io
sudo python3 tools/hardware_trackpad.py run --run <run>
python3 tools/hardware_trackpad.py collect --run <run>
```

The v13 profile binds the I/O trial to the same run identity and enables the
existing gesture policy. An early exit, missing input category, workload failure,
missing/corrupt record or payload mismatch cannot pass the combined gate.

## Verification

Host tests exercise exclusive temporary/result creation, patterned write/rename/
read, checksums, partial pipe reads, the terminal-message/exit race, malformed
progress and the stop deadline. Python tests reject corrupt evidence, payload
mismatches and missing activity. Existing desktop tests cover capture, focus,
queue pressure and client cleanup.

```sh
tools/host-env.sh make -j4 all test-desktop test-trackpad-capture test-desktop-io
PYTHONPATH=tools python3 -m unittest tools/test_desktop_io_result.py tools/test_hardware_trackpad.py tools/test_hardware_boot.py
tools/host-env.sh python3 tools/trackpad_capture_smoke.py --desktop-io
```

QEMU uses the real worker on disposable USB exFAT with emulated input, runs at
least 16 rounds, checks keyboard/motion/focus/cancel counters, validates the final
payload and clean volume, and retains replay/read-only refusal checks. Missing
physical Apple trackpad coverage remains INCOMPLETE; emulated input is not a
substitute for the laptop trial. Device removal/recovery, quantitative latency
and palm-classifier accuracy remain separate gates.

The combined QEMU run completed 16 rounds and stopped gracefully, with 10 text
characters, 19 motion events, two focus changes and one captured-drag cancellation.
The final persisted payload matched and the offline exFAT check was clean.
The smoke harness now drains serial continuously during QMP input injection;
pausing host reads had triggered the guest UART timeout and hidden continued
worker progress. This was a harness observation failure, not evidence of a
filesystem deadlock. The on-screen checklist and live counts were inspected at
1280x800. Physical acceptance remains pending.
