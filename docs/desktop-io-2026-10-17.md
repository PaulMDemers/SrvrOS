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

## First physical result and bounded optimization plan

Run `9827a0816f9a01a1`, release `860b0e14f63f89e8`, returned to Ubuntu after
manual testing. User feedback: significant stuttering during heavy I/O, but
interaction continued to work. This is a functional result, not responsiveness
acceptance; retain the earlier accepted checkpoint.

Collection validated the report/completion marker, both auxiliary records and
final persisted payload. The worker stopped gracefully after 126 rounds
(33,030,144 logical bytes / 31.5 MiB). During its RUNNING interval the compositor
recorded 58 characters and 325 motion events, but zero wheel events, focus changes
or captured-drag cancellations. Overall capture recorded 2,015 valid trackpad
reports, 23 scroll events, zero malformed/transport errors, zero dropped keyboard
characters and a successful desktop exit. Overall gesture coverage does not
establish scroll/focus/capture coverage during the workload: the combined gate
is correctly INCOMPLETE.

Report SHA-256: `aa3955f4aed8582f9faf8c021be8b2910fd208e0db70c5231aad4cc2324ecd9e`.
Final payload SHA-256: `8e479b18bbcec08f7e18e973ced31ec4a6bd63943bab884c90039523d2171eea`.
Host evidence: `build/trackpad-tests/9827a0816f9a01a1/`.

The first code-level suspect is the global I/O ownership gate. exFAT retains
ownership across a complete mutation. USB bulk waits yield, but
`syscall_io_safe()` excludes desktop IPC, mouse scanning, surface operations,
presentation and several compositor housekeeping calls. Therefore yielding the
CPU does not itself let the desktop complete an update. Allocation-bitmap and
metadata-sidecar writes also amplify traffic; their contribution to the observed
stutters has not yet been measured.

Recommended remaining scope, in order:

1. Measure active desktop service/presentation gaps, syscall I/O-gate wait time,
   I/O ownership duration and USB command/byte/flush counts. Report distributions
   and maxima, separating normal idle time from active interaction. Preserve a
   comparable baseline before changing scheduling.
2. Keep filesystem mutation serialization while permitting audited bounded input,
   desktop messaging and presentation work during storage waits. Review shared
   allocator, process lifetime, surface ownership and queue invariants first;
   blindly marking every GUI syscall safe is not a solution. Compositor worker
   progress/liveness checks must also avoid reintroducing a blocking dependency.
3. If measurements still show long uninterrupted work, reduce redundant bitmap /
   sidecar traffic and add bounded service opportunities between transfer batches.
   Preserve fsync, clean-marker ordering and error quarantine. Do not improve the
   benchmark by reducing the workload or acknowledging saves early.
4. Repeat the combined test with all activity categories observed while RUNNING,
   then a bounded longer run and independent payload/offline filesystem checks.
   Regress slow/dead clients, queue pressure and storage failure behavior.

Proposed exit gate: normal redraw retains the 16.7 ms target; during active input
under the same USB workload, desktop service/presentation gaps should be below
50 ms at p95 and 100 ms at p99, with no unexplained gap above 250 ms. These are
engineering targets, not measurements or physical input-to-photon claims. Also
require no lost input/stuck capture, verified durable data, clean offline exFAT,
and user confirmation that the pronounced stuttering is gone. If a flash-device
flush itself takes longer, the desktop must remain serviceable while it waits.

Stop when that gate passes. Full asynchronous storage, a general filesystem
cache, GPU acceleration, a scheduler redesign and peak USB throughput tuning
are separate projects unless measurements prove one necessary for this gate.
Then proceed to the shared controls and useful Files/Text workflow in D3.

## Responsiveness implementation (physical validation pending)

The timed profile is now v14. `SYS_MONOTONIC_US` returns reference-clock
microseconds, or -1 if unavailable. The compositor records a fixed histogram of
loop-completion gaps only when the current or previous loop handled input while
the worker was RUNNING; quiet idle intervals are excluded. Render measurements
cover compositor raster/present work, not the client's raster or physical scanout.
Percentiles are conservative histogram upper bounds, capped by the observed
maximum, rather than exact order statistics. `timing.txt` is checksummed and
bound to the run. Collection requires at least 100 active samples for the timing
gate, in addition to the existing input/workload coverage requirements. The
collector also requires compositor render/present p95 <=16.667 ms.

Kernel instrumentation, enabled only for this trial, records I/O ownership and
explicit syscall gate waits, plus USB command/data-byte/flush counts. Owner time
includes time spent servicing other tasks. Gate counts exclude time a thread
was simply ineligible to run before reaching the gate; neither metric is a
complete CPU-time profile. USB bytes are requested data-phase bytes across the
whole measured desktop session, including startup and evidence writes.

The guarded paths have these boundaries:

- GUI messages, mouse scanning, surface operations and framebuffer blits can run
  during another process's storage wait. Queues/surfaces are protected by IRQ
  exclusion; copy chunks are private; heap allocation is IRQ-protected. No new
  permission bypasses surface ownership, sealing, size or user-buffer checks.
- Process listing is a read-only snapshot. Bounded sleeps remain schedulable.
  Process exit, kill, spawn, mappings and ordinary filesystem operations remain
  gated; sibling threads of the I/O owner remain excluded.
- Explicit nonblocking pipe reads return AGAIN during foreign I/O. They do not
  consume bytes/file rights or run descriptor cleanup then. When admitted without
  contention, their continuation is marked unsafe before reading. Redirected
  stdio and ordinary file/network reads do not gain this exception.
- Nonblocking waits return "not ready" during foreign I/O and defer reaping.
  Ordinary reaping retains unsafe eligibility, and the nonblocking path no longer
  enables interrupts before its quick check. Blocking waits retain their behavior.
- Successful USB commands offer a service opportunity after a cumulative 4 ms
  owner budget. This closes the gap where many individually fast transfers each
  restarted their local timer. Existing longer-transfer wait service remains at
  approximately 1 ms. Filesystem ownership is retained across these yields.

The compositor now keeps two reusable maximum-size private caches per window.
A failed partial surface copy never replaces the front cache. With two 960x640
windows this reserves 9.375 MiB for caches; each study also reuses one 2.344 MiB
raster buffer across resizes. These bounded allocations replace per-commit
mmap/munmap churn. Allocation failure cleans up the whole cache reservation.
Filesystems, flush ordering, transport quarantine and the 256 KiB workload are
unchanged. Metadata-write optimization remains deferred unless hardware timing
shows it is necessary.

The on-screen trial now shows WAIT/OK coverage for scrolling, two focus switches
and captured-drag cancellation, plus the active timing sample count. The trial
still saves and halts; timing/coverage failures cannot silently count as acceptance.

Instrumented QEMU baseline (16 rounds, 1280x800): 51 active samples,
p95 <=250 ms, p99/max 296.752 ms, longest explicit gate wait 413.245 ms in
SYS_GFX_BLIT_RECT. Requested USB data was 92,173,824 bytes over 24,341 commands,
including 666 flushes. This demonstrates write amplification but includes all
session overhead; it is not a payload-throughput measurement. Baseline artifacts
are retained under `build/review/desktop-io-baseline-20261017/`.

Final short QEMU comparison with the 4 ms cumulative service budget: 58 active
samples, p95/p99/max <=28.032 ms; 110 render samples, p95/p99/max <=16.179 ms.
USB counts/bytes/flushes exactly matched the baseline, and the independently
read payload and offline exFAT check passed. Ordinary desktop regression at
1440x900 also passed mixed-case input, text size, drag, resize, exit/relaunch and
one idle presentation. These are emulator results, not hardware acceptance.

The parent now waits for pipe EOF as well as worker exit before judging a missing
terminal record. A deferred nonblocking read must not turn a successfully exited
worker into a false failure. Tests cover this race, histogram thresholds and idle
exclusion, pipe admission/deferral, same-owner thread exclusion, cumulative service
budget, cache reuse and partial-copy/allocation failure cleanup. Full core tests
passed; cache and trial-state tests also receive ASan/UBSan checks.

The extended 64-round QEMU run verified 16 MiB of logical payload with repeated
input during rounds 17–64: 974 active samples, p95/p99/max <=38.972 ms; 1,543
render samples, p95 <=16.667 ms, p99 <=25 ms, maximum 27.176 ms. Both final-payload
verification and offline exFAT checks passed, as did replay/read-only refusal and
halt behavior. This satisfies the software timing thresholds in this emulator;
physical scrolling coverage and feel still require the laptop trial. Evidence:
`build/review/desktop-io-trial-64/`.

The physical USB volume from the prior 126-round run was separately unmounted
and checked with `fsck.exfat -n`: clean, 124 directories and 615 files. It was
remounted through udisks afterward. No filesystem repair was performed.
