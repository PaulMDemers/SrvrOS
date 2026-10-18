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

## Physical timed result and text-path follow-up (2026-10-18)

Run `7c6f08f4dc58ebac`, release `448fd9b297dd65bc`: the user reported that
text input appeared blocked by I/O while the trackpad cursor stayed fluid.
Collection verified 66 rounds / 16.5 MiB of logical payload, graceful worker stop,
89 text events, 2,720 motion events, 55 wheel events, one focus switch and no
capture cancellation during RUNNING. The combined gate remains INCOMPLETE.
There were no malformed trackpad reports, transport errors or dropped keyboard
characters; the final `AaZz09` check passed.

The 3,006 active compositor samples had p95/p99 <=16.667 ms, but maximum
987.437 ms. Render/present maximum was 2.018 ms. The timing gate correctly says
NEEDS_WORK. Kernel ownership maximum was 525.477 ms; the longest explicit gate
wait was 1,714.650 ms in SYS_OPEN. These aggregate metrics do not attribute the
text stall or identify which process opened a file. Loading a second study while
file work is running also uses the serialized filesystem path.

Report SHA-256: `7babf16ef8e986b6e951a9c0b24cbfa247f73e61c8adfc40d6dfd8d67138ab96`.
Payload SHA-256: `9ba6a7aade4c43fb9b7bd965e6c4badb10ee03c6d7012338e54d0c9634aee762`.
Evidence: `build/trackpad-tests/7c6f08f4dc58ebac/`. After collection, offline
`fsck.exfat -n` reported clean: 125 directories, 622 files. The volume was
remounted through udisks. The accepted `e276699c454972fd` backup still verifies.

Code inspection found a separate blocking dependency on every text event:
`deskclient` called printf before drawing. Stdio locks call pthread_self, which
uses the gated SYS_THREAD_SELF; write's libc wrapper also queries gated signal
state, and first use may allocate a stream buffer. Compositor progress logging
used the same path. These are incidental diagnostic dependencies, not required
filesystem operations. Desktop logs now format in a fixed stack buffer and call
srv_puts directly. This retains the diagnostic markers used by emulator tests,
without stdio initialization, lock queries or signal queries in the input path.
Redirected output is still subject to the ordinary kernel I/O gate. No kernel
admission rules, filesystem ordering or USB workload guarantees were relaxed.

The v15 trial loads both studies and receives their initial surfaces before
starting file work. This separates application startup from steady interaction;
application launch, mapping and process teardown remain serialized and can still
pause a caller during I/O. A full asynchronous launch path is outside this fix.
Escape during startup cancels the trial rather than starting a workload later.

Desktop protocol v2 keeps the 160-byte packet size, replacing eight title bytes
with input_serial. Unknown protocol versions are rejected by both kernel and
library. For the I/O trial, each accepted text event gets a sequence and reference
clock timestamp before its first IPC attempt. The client echoes the cumulative
sequence in the surface commit after processing input. Only a valid copied
surface followed by a successful compositor presentation cycle acknowledges it.
This measures the application round trip, including IPC retries, client drawing,
surface copy and compositor presentation. It excludes time before the compositor
reads the key and physical scanout; occlusion can still hide a window's content.
It is not an input-to-photon measurement or proof of correct text layout.

A fixed 256-entry pending table tracks each event independently, including keys
batched into one frame. Cursor-only frames, another client's frames, stale or
failed copies cannot acknowledge new text. Missing acknowledgements and table
overflow fail the new host gate. Checksummed `text.txt` requires at least 32 text
samples, all acknowledged, no overflow, p95 <=50 ms, p99 <=100 ms and maximum
<=250 ms. The prior compositor/render gate and functional coverage still apply.
Old v14 evidence remains readable without pretending it contains text timing.
The on-screen checklist now starts with two studies ready and shows the text
sample count. Hardware acceptance remains pending.

Follow-up validation: full core tests and 29 Python tests passed. ASan/UBSan
checks passed for pending text timing and compositor cache/session handling.
Tests cover batched acknowledgements, wrong-client/future acknowledgements,
duplicate acknowledgements, an intentionally delayed text frame, table overflow,
stale/partial surface copies and protocol-v1 rejection.

The 64-round emulator run verified its final 256 KiB payload after 16 MiB of
logical work and a clean offline filesystem. All 234 measured text events were
acknowledged: p95 <=50 ms, p99 <=100 ms, maximum 116.011 ms, zero table overflows.
There were 1,057 active compositor samples (p95/p99 <=50 ms, maximum 74.051 ms)
and 1,665 render samples (p95 <=16.667 ms, maximum 64.099 ms). Both software
timing gates passed. This run included a concurrent ordinary desktop regression
on the host; these emulator numbers are not a direct hardware prediction.
The ordinary 1440x900 compositor regression passed typing, scrollbar dragging,
two windows, focus, resize, client replacement, abrupt server exit and relaunch.
The updated on-screen checklist was visually inspected at 1280x800.
Replay/read-only refusal and indefinite halt also passed. Follow-up emulator
evidence is retained at `build/review/desktop-io-text-20261018/`.

## Smooth physical result; final coverage outstanding

On 2026-10-18 the user reported that both keyboard and trackpad input stayed
smooth in run `fa73dec75850d130`, release `c24b450ed120c323` (commit `e87f22b`).
Both timing gates passed: all 98 accepted text events reached an application
frame, p95 <=16.667 ms, p99/max 21.258 ms, zero pending-table overflows. Across
1,915 active compositor samples the maximum gap was 14.599 ms; across 2,046
render samples the maximum was 1.608 ms. There were zero dropped keyboard
characters, malformed trackpad reports, transport errors or pointer overflows.
The final `AaZz09` check and desktop exit passed.

The worker stopped cleanly after 13 rounds (3,407,872 logical bytes). Ubuntu
independently verified the final payload. Offline `fsck.exfat -n` reported clean:
126 directories, 630 files; udisks remounted the volume afterward.
Report SHA-256: `c5fd85521c313c102594efd8ee630b2006d0cf4cb6deb1293cb4b275b70b9ca3`.
Payload SHA-256: `e5ec6b49506e06842082aa65dfb3452b60d9aad00acb92c75580b899b2beae36`.
Evidence: `build/trackpad-tests/fa73dec75850d130/`.

This establishes the responsiveness improvement on the target laptop. It does
not complete the combined gate: the workload stopped before 16 rounds, only one
focus switch was counted, and captured-drag cancellation was not exercised.
Keep these distinctions in the results; do not lower thresholds after the run.
A verified backup with that explicit acceptance scope is stored at
`/home/paul/.local/share/srvros/checkpoints/c24b450ed120c323/`; the prior fully
accepted desktop backup remains unchanged.

The follow-up changes only test guidance: show the next missing action at the
top of the checklist, explicitly request a physical click-and-hold on the
scrollbar thumb while pressing Tab, and warn on early Escape. A consecutive
second Escape still permits an incomplete exit; other input clears the warning.
Worker failure/completion and startup cancellation remain escapable. The host
still independently checks timing, coverage, checksums and payload contents.

Guidance validation passed the desktop and I/O host suites and a 16-round QEMU
trial. The emulator verified the early-Escape warning, deliberate second-Escape
exit, final payload, clean offline exFAT, replay/read-only refusal and halt.
Its deliberately incomplete physical-input coverage remains INCOMPLETE.


## Full workload completed; concurrent-input coverage absent

Run `a08397c25b269821`, release `66dd9f876af60696` (commit `943f528`),
completed all 128 rounds / 32 MiB of logical work. The user reported everything
appeared to work well. Ubuntu verified the final payload, the report and its
completion marker. Offline `fsck.exfat -n` reported clean: 127 directories,
638 files; the volume was remounted afterward. Overall capture recorded 3,740
valid reports, 469 scroll events, no malformed reports or transport errors,
and a passing keyboard/desktop-exit result.

However, every input-during-workload counter was zero, with zero active-loop
and text timing samples. This run establishes a complete storage workload,
but does not establish concurrent interaction or new responsiveness timing.
The previous run's smooth physical feedback and timings remain valid within
their recorded scope. Clarification is pending on whether interaction began
after the workload finished; do not classify this as a responsiveness regression
or promote it as combined acceptance without that missing evidence.

Collection initially rejected the zero-sample timing record as inconsistent.
The host validator now accepts a zero-sample histogram only when all its timing
values are zero, preserving the evidence as incomplete. Acceptance still requires
100 active samples and nonzero render samples; zero text samples also cannot
pass. Thirteen collector/evidence tests passed, including the new empty-histogram
case and rejection of timings reported without samples.

Report SHA-256: `2d688458dba98fd306eb485c63339e1288bad4dfa2098a5d7df5ac646c105390`.
Payload SHA-256: `a3f1a4bb482cd88b6fb2a67190bd3b9c06bd39c1ab77155a166d0231e04bdb72`.
Evidence: `build/trackpad-tests/a08397c25b269821/`.

## User-started trial and workload timeline (v16)

The Ubuntu journal placed the previous reboot at 2026-10-18 00:26:57 EDT and
Ubuntu's return at 00:41:06 EDT. SrvrOS's capture lasted 801.326677 seconds,
including the desktop and later capture screen. Neither that duration nor the
unusable exFAT wall-clock timestamps identifies when the worker finished.
The user confirmed interacting while the desktop was visible but was unsure
whether file work was still running. Keep the no-overlap result unaccepted.

The v16 I/O trial now loads both studies, displays READY, and waits indefinitely
for Space before spawning the file worker. Space is consumed as the start action;
it is not a text sample. Escape before starting returns without running the
worker. Normal desktop launches retain their existing behavior. The completed
workload now explicitly tells the user to save, rather than requesting checks
that can no longer be counted during file work.

Two additional checksummed records use the same monotonic reference clock:

- `work-clock.txt`: the worker timestamps its own test-loop start and finish.
  This interval includes the existing inter-round pacing and ends before writing
  the result/evidence files. It is not pure device service time.
- `input-clock.txt`: start request, first non-Escape key after starting, first
  meaningful pointer event, last input, optional stop request, and the timestamp
  when this record is prepared. Key events include controls such as Tab. First
  input is measured when the compositor reads it, not at physical actuation.
  Input after worker completion is also recorded, making late testing visible.

The collector requires both records for v16, checks their checksums, identities
and timestamp ordering, and prints workload duration plus whether the first key
and pointer event were before, during or after the worker interval. Zero optional
input timestamps mean no event, not time zero. Older trial profiles remain
collectable without inventing timeline evidence. Existing coverage and latency
acceptance thresholds are unchanged.

Validation: the build, desktop/I/O host tests and 30 Python tests passed. The
emulator verified a six-second READY wait without spawning the worker, explicit
Space start, early-Escape warning/override, persisted timeline and payload,
clean offline exFAT, replay/read-only refusal and indefinite halt. The first
attempt was terminated during the final read-only boot; the complete retry
passed. In that retry the worker interval was 24.475974 seconds, first key
0.613341 seconds after its start and first pointer input 2.022013 seconds after.
The short emulated run deliberately remains incomplete for physical coverage.
The READY screen was visually checked at 1280x800. Evidence is retained under
`build/review/desktop-io-manual-start-20261018/`.
