# Console, input, timing and storage implementation

This is the first non-GUI implementation batch from the
[architecture review](optimization-review-2026-10-16.md). GUI work is deferred.
The laptop boot and throughput check passed on 2026-10-16; the wider hardware
and porting roadmap is not complete.

## Preserved baseline

Source checkpoint: `b79c5593d09952d6a1b334db0abe00c75db58807`.
The original laptop-confirmed payload is `c02ec72ad1c8f3d4`. A verified copy is retained at
`/home/paul/.local/share/srvros/known-good/c02ec72ad1c8f3d4`, outside the installer's
current/previous rotation. The user installed and booted the optimized release
`3cd2e11a98996ccb`; it now has its own verified independent copy at
`/home/paul/.local/share/srvros/known-good/3cd2e11a98996ccb`. Installed state retains
the original release as `previous` and the optimized release as `current`.

## Implemented

- The console has all 95 printable ASCII glyphs, including distinct lowercase
  and braces. The indexed font lives in `shared/include/srvros/font5x7.h`.
  The GUI's old font is deferred with the GUI work.
- Console text and scrolling live in fixed RAM storage. Glyph tiles are cached;
  dirty spans are written forward to the framebuffer without reading it.
  The viewport supports up to 512 columns and 256 rows at 8×16 pixels per cell.
- Normal console output coalesces through the workqueue. Model changes use
  64-byte batches; presentation releases interrupts between scanlines. No heap
  allocation is performed per glyph, write, or presentation. Queue-full fallback
  and fatal-exception output remain synchronous. The presenter respects display
  ownership changes, including a claim that occurs while it is rasterizing.
- Console `write` syscalls copy validated 256-byte user chunks before permitting
  interrupts; subsequent chunks are revalidated. This permits timer/input service
  without continuing to access a mutable user pointer while preemptible.
- USB keyboard state is per device: both Shift keys, Caps Lock/Shift interaction,
  duplicate suppression, rollover handling, editing sequences and Ctrl-C signals.
  Held keys repeat after 500 ms at approximately 30 Hz. PS/2 gains Caps Lock and
  correct simultaneous Shift tracking. Caps Lock LEDs are not implemented.
  Repeat stops on root-port disconnect; runtime enumeration/hub disconnect
  recovery remains future work.
- An ACPI PM I/O timer provides a reference clock; LAPIC ticks calibrate to 100 Hz.
  Unsupported/stopped references are reported explicitly. The 24-bit counter is
  extended by periodic sampling: intervals longer than its roughly 4.7-second
  wrap without a sample, including suspend, are not supported yet. The fallback
  tick clock is not claimed to be calibrated.
- The long input probe is opt-in. Init reports startup completion using syscall
  117 (`SYS_PROC_READY`); the kernel waits for that PID's readiness or a bounded
  three-second tick deadline instead of always sleeping three seconds. Waiting
  is skipped if the boot interrupt check observed no timer ticks. Normal
  hardware staging is quiet; diagnostic staging retains the full USB snapshot.
- AHCI registers are volatile; busy/command polling has reference deadlines and
  a separate iteration bound. A timed-out/failed disk is quarantined, its DMA
  buffers stay pinned, and subsequent I/O fails. Reset/recovery is not attempted.
  Flush commands use no data descriptor and are selected from IDENTIFY support.
- `sync` and `fsync` reach device cache flushes. Writable devices without a flush
  implementation return failure. `fsync` conservatively flushes all writable
  devices, so an unrelated failed disk can also make it fail. RAM filesystems
  remain volatile. ATA support-field validation follows the
  [Linux ATA definitions](https://github.com/torvalds/linux/blob/master/include/linux/ata.h).
- Failed cached writes invalidate the affected block. Partition aliases share
  their parent's cache. A failed filesystem overwrite no longer deletes the
  existing file and retries. This does not make exFAT crash-atomic or undo a
  partially completed write.

Early-boot tests caught compiler-generated SSE before CPU initialization. The
console, memory primitives and FPU initialization now compile without SSE;
normal kernel/user FPU support remains enabled after initialization.

The reference timer layout follows the
[ACPI FADT specification](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html).

## Measurement and validation

`renderbench` at the monitor writes 1,000 lines of 80 printable characters,
flushes pending output and reports elapsed microseconds, scrolls, presentations,
and framebuffer bytes written. Serial mirroring is disabled during this timed
workload; `renderbench serial` includes serial cost. `renderstat` prints lifetime
counters. Benchmark text remains in the boot-log ring.

Reproduce the host comparison with:

```sh
python3 tools/console_benchmark.py --baseline b79c559 --samples 3
```

It compiles the actual baseline/current consoles, draws into a 1440×900 RAM
framebuffer and stubs serial. This measures CPU/RAM work and coalescing, not
uncached device memory or interactive key-to-screen latency. Results are saved
in `build/review/console-host-benchmark.json`. Earlier samples of this batch were
roughly 0.9–1.2 seconds before and 6–8 ms after, with one final presentation; do
not translate that ratio into a promised laptop speedup.

The completed checks are recorded under `build/review/`:

| Check | Result / evidence |
| --- | --- |
| Host console, keyboard, clock, block, AHCI and xHCI tests | PASS, `host-core-tests.log`; includes panic/muting fallback, padded framebuffer guards and fault injection |
| Installer compatibility/rollback tests | 8 PASS, `installer-tests.log`; all four configuration versions verify |
| Serial harness synchronization tests | 3 PASS, `cli-harness-tests.log` |
| QEMU syscall ABI | PASS, `non-gui-sysabi-final.log` |
| QEMU shell editing and filesystem check | PASS, `non-gui-shell-edit.log`; zero filesystem errors |
| QEMU 479-command CLI/process/filesystem regression | PASS, `non-gui-cli.log`; 384 files, zero leaks/FAT errors/filesystem errors |
| GRUB → Limine → quiet RAM boot, USB case/Caps/repeat, calibrated clock/readiness and rendering | PASS, `console-grub-final.log` and `hardware-grub-serial.log` |
| Visual inspection | `console-performance.png`; lowercase, capitals, punctuation and the final prompt render correctly |

The full CLI run used storage/console candidate `fe88db1e72bd851b`; the later
init-readiness, shorter output batches and presentation-ownership refinements
received host tests and focused boot, USB, rendering and ABI checks. The shell
editing run used `9e40e0af30c4b7bf`. No full GUI regression suite was run.

At 1280×800, a focused QEMU run of the final console produced 1,000 80-column
lines in **32,231 µs**, with 965 scrolls, **2 presentations**, **8,192,056 bytes
written**, and **zero framebuffer bytes read**. Init readiness took 83,239 µs in
that run. CPU scheduling and initial cursor position affect these values. These
are one-run QEMU observations, not laptop latency percentiles or acceptance of
the physical performance target.

The final host RAM comparison had baseline samples of 856,400 / 904,931 /
1,111,891 µs and current samples of 5,739 / 6,468 / 6,217 µs. Each current host
sample used one final presentation and wrote 5,160,988 framebuffer bytes.
Kernel timing/output tests provide a separate check because the host build and
its stubbed scheduler/serial are not equivalent to the booted kernel.

Final payload: **`3cd2e11a98996ccb`** (normal quiet configuration),
verified through GRUB/OVMF/QEMU 10.2.1 with 2 GiB guest RAM, then installed and
booted on the laptop by the user. The hardware entry uses the existing read-only initramfs
block device; filesystem mutation tests use disposable AHCI disk images.

## Physical laptop result — 2026-10-16

The user reported that the MacBookAir7,2 booted "instantly, way faster."
The supplied photograph shows mixed-case output and this completed benchmark:

| Measurement | Observed value |
| --- | --- |
| Workload | 1,000 lines × 80 columns, serial mirroring off |
| Elapsed time | **4,889 µs (4.889 ms)** |
| Framebuffer | 1440×900, pitch 6144 bytes |
| Logical scrolls | 951 |
| Presentations | **1** |
| Framebuffer bytes written | 5,160,988 |
| Framebuffer bytes read | **0** |

This confirms the batching path on physical hardware: scrolling updates the RAM
text model, then the final viewport is presented once. It does not redraw the
framebuffer for every logical scroll. This is one throughput sample, not a
measurement of interactive latency or boot duration. There is no measured old
laptop baseline from which to calculate a physical speedup ratio. Caps Lock and
held-key repeat were not separately confirmed in this report.

The subsequent `renderstat` reports lifetime totals, separate from that benchmark:
88,984 bytes, 954 scrolls, 35 presentations, 45,271 cells, 23,179,732 framebuffer
bytes written and zero read. Evidence is the user-supplied `Photo 1.jpg`
(attachment `8AF50B55-111E-4C62-AEB2-45F60CD88AB8`); structured measurements are
also retained in `build/review/non-gui-validation.json`. Installed payload hashes
and both independent rollback copies were verified after the report.

## Follow-up: sustained output and latency

The next candidate removes serial transmission waits from the console's
text-model critical section. UART readiness checks and individual byte writes
remain atomic; waits restore the caller's interrupt state between attempts.
Early boot and callers already holding interrupts disabled retain that state.
Concurrent serial writers can interleave at byte boundaries; serial remains
synchronous and timeout-bounded by an iteration limit.

The workqueue now processes pending callbacks in FIFO order, preserving
coalescing by callback/argument. A callback that requeues itself goes behind
existing work. The worker yields after every callback, and scheduling fails if
worker creation failed so callers can use their fallback paths. Callback bodies
still need their own bounds; this is not priority scheduling.

`latency reset` enables reference-clock sampling of console, heap, AHCI and
serial critical sections, plus timer-delivery and USB-worker polling gaps.
`latency off` freezes the counters and prints them. Sampling is off by default.
Critical-section samples are local to the named subsystem, not whole-kernel
IRQ-off coverage; cross-site samples may overlap. Gaps include scheduling and
emulator delays and are not input-to-pixel measurements. AHCI remains disabled
in the normal laptop configuration. Its synchronous polling/address-space and
DMA ownership rules need a separate change before interrupts can safely be
enabled while waiting for storage.

`renderstress` continuously writes and scrolls for three seconds, yielding
between 32-line batches and collecting keyboard input. Type `aAzZ123` during
the run; the final report includes captured text, timer ticks and dropped-key
count. Escape cancels it. Serial mirroring is suppressed for the workload, as in
the default `renderbench`; the separate userspace `yes`/Ctrl-C smoke exercises
output with serial mirroring enabled. See [testing](testing.md) for reproduction
and measurement limits. Physical input validation is recorded below.

Candidate **`70ddde650d0fc3f8`** passed nine host test groups, eight installer
tests, the QEMU syscall ABI smoke, and the shell-editing smoke with zero
filesystem errors. The expanded GRUB/OVMF smoke passed mixed-case/Caps/repeat
checks, sustained USB typing, Escape cancellation, and USB Ctrl-C during
userspace `yes` followed by a successful shell command.

In the instrumented QEMU run, `renderstress` wrote 11,008 lines in 3,000,246 µs,
delivered 299 timer ticks, and captured all seven `aAzZ123` characters with zero
keyboard drops. Over the full sampling window (including command entry), the
largest observed console section was 5,506 µs, heap section 65 µs, and serial
section 62 µs; AHCI had no samples. Maximum timer-delivery and USB-poll gaps were
34,928 µs and 39,521 µs. These are single-run emulator observations with sampling
overhead, not physical latency guarantees. The uninstrumented `renderbench`
completed in 39,314 µs with two presentations and zero framebuffer reads.

Evidence: `build/review/latency-host-tests.log`, `latency-installer-tests.log`,
`latency-grub-smoke.log`, `hardware-grub-serial.log`, `console-latency.png`,
`latency-sysabi.log`, and `latency-shell-edit.log`. The full 479-command CLI suite
was not repeated for this follow-up.

On 2026-10-16, the user booted release `70ddde650d0fc3f8` on the MacBookAir7,2
and reported that the sustained-output test captured all typed text without
problems. The camera did not save the photograph, so no physical latency or
drop-counter values are recorded. This confirms successful input under the tested
load by user observation; Escape and Ctrl-C remain QEMU-validated only for this
batch. Installed state confirms `70ddde650d0fc3f8` as current and
`3cd2e11a98996ccb` as previous. The new release was hash-verified and copied to
`/home/paul/.local/share/srvros/known-good/70ddde650d0fc3f8` independently of the
installer's current/previous rotation.

## Repeating the laptop test

From the repository in Ubuntu, install the staged candidate, then request a
one-shot boot:

```sh
sudo python3 tools/hardware_boot.py install
sudo grub-reboot srvros-current
sudo systemctl reboot -i
```

At `srv>`:

```text
echo aAzZ []{}~!
renderbench
renderstat
run /fat/bin/sh
```

Check Caps Lock, Shift+Caps Lock, holding a letter, releasing it, and Backspace.
Compare scrolling and command editing with the previous boot. Photograph the
`renderbench` result; it includes mode/pitch so later measurements are comparable.
Use `exit` to return from the shell. The filesystem remains RAM-backed in the
hardware entry, and the installer retains the previous boot release.

To stage the opt-in diagnostic configuration instead:

```sh
python3 tools/hardware_boot.py stage --diagnostic
```

That selects verbose logging, the USB diagnostic snapshot and the deliberate
input probe. Re-run `make hardware-stage` to return to the normal configuration.
Both configurations preserve verification of older v2/v3 releases.

## Remaining non-GUI work

1. Extend the confirmed laptop throughput result with sustained-output and
   interactive-latency measurements; record framebuffer cache attributes.
   Check PAT/MTRR and mapping aliases before changing write-combining policy.
2. Extend deadline/idle-wait conversion beyond LAPIC/AHCI, measure maximum
   interrupt-disabled intervals, and reduce allocator/network/storage critical
   sections using explicit ownership. AHCI command polling is still synchronous
   with interrupts masked; the new timeout bounds failure, not normal latency.
3. Add deeper exFAT fault/power-loss recovery tests before enabling persistent
   hardware storage. Reboot tests currently use RAM storage deliberately.
4. Implement the Apple trackpad's separate USB interface, followed by physical
   networking and laptop power management, each with its own hardware validation.
5. Consolidate large subsystems and publish verified API/Node/libuv compatibility
   status. GPU acceleration, Unicode, SMP and demand paging remain later work.
