# Input during filesystem work

The USB batching checkpoint passed five physical rounds with a mean shell
workload time of 8.048 seconds. It reduced throughput overhead but still left
the shell unable to echo during a long filesystem mutation. This increment
separates terminal progress from filesystem ownership without allowing concurrent
filesystem mutations.

## Ownership and lifetime rules

The existing nested I/O owner remains responsible for filesystem transactions,
block-cache publication and USB buffers. It now has a wait queue: competing
unsafe syscall entries sleep until the outermost owner scope ends. A nested
release does not wake waiters. Contention does not return a synthetic I/O error.

Threads running ordinary userspace or audited console syscalls in a different
process may run during owned I/O. Every syscall entry first revokes that
eligibility. The admission policy allows actual console reads/writes, terminal
attributes/window-size operations, console positioning/info, key scanning,
yield, PID and tick queries, and process-local futex wake (used by stdio
unlock, with no allocation or resource cleanup). Redirected file/pipe I/O is excluded. The policy
defaults to waiting for all other calls, including open, close, sync, poll,
mapping changes, spawn, kill and exit. In particular, a mutable userspace poll
array is not pre-classified and then trusted by a later copy.

Kernel continuations suspended in other syscalls remain excluded until ownership
is released. The owner's sibling threads are also excluded, protecting shared
user buffers and mappings. Existing kernel services retain their restricted
eligibility checks; this does not admit arbitrary kernel workers during I/O.

Timer cancellation is deferred while any I/O scope is active. Syscall boundaries
wait before process teardown, including a cancellation received while waiting
for ownership. Process/fd cleanup therefore cannot run against another task's
in-flight filesystem operation. This deliberately favors completing an owned
transaction over immediately terminating its process. Existing device deadlines
and quarantine behavior remain necessary for stalled hardware.

This is conservative kernel-entry admission, not per-file or per-volume
concurrency. A program that requests storage, allocation or another gated
operation can still wait behind the owner. Console typing and echo can proceed.

## Keyboard pipeline

Successful HID completions now rearm the interrupt endpoint immediately after
consuming the report, including completions consumed by a bulk-storage waiter.
The DMA buffer is cleared only after the report has been decoded/copied. Old
TRB events cannot consume the replacement request. Error completions retain
the existing worker retry behavior.

The initial test injected long bursts at five milliseconds per character,
including separate Shift transitions. That can exceed the virtual device's
report rate and overflow QEMU's input queue. The accepted stress test uses
25 ms for ordinary characters and 50 ms for shifted characters, checks exact
saved text, and rejects a run containing `hid_kbd_queue_full`. The monitor
`latency` command now also prints keyboard queue counters, and this test requires
zero dropped keys. It waits for both the background completion and a fresh
foreground prompt after saving the typed text before sending another command. QEMU's
[keyboard descriptors](https://github.com/qemu/qemu/blob/master/hw/usb/dev-hid.c)
and [HID event queue](https://github.com/qemu/qemu/blob/master/hw/input/hid.c)
document the interval and overflow behavior. The immediate-rearm change does
not claim to overcome that device limit.

## Shell argument correction

The contention script exposed an existing truncation in wildcard processing:
quoted arguments used 160-byte pathname buffers, silently cutting a longer
`sh -c` script. Those buffers now use the shell's expanded-argument capacity.
The contention test requires the trailing completion marker as well as exact
contents from both writers and the reader/copy operation.

## Validation scope

Host tests exercise owner nesting, service/user eligibility, exclusion of owner
siblings and unsafe kernel continuations, actual wait/wake, syscall default-deny,
console versus redirected descriptors, cancellation before/after calls and while
waiting, and immediate HID rearm with stale-event rejection. Existing block,
USB, filesystem, busy-unmount and flush-failure tests remain in the core suite.

The expanded QEMU workspace test requires key echo below 100 ms while the
virtual disk has accepted at least 512 KiB of writes and the background
writer has not reported completion. It checks mixed case, Caps Lock
with Shift, both Shift keys, punctuation, cursor editing, Backspace and held-key
repeat/release through the emulated USB keyboard. It verifies concurrent writers
and file readback, then interrupts a foreground writer after observing substantial
USB writes and requires status 130, complete saved contents and clean filesystem
checking. These are host-clock emulator checks, not physical display latency.

The automatic laptop shell workload now starts its 512 KiB writer in the
background, updates the independently checked settings file, prints console
output and waits before flushing. Expected output files and report format stay
the same; elapsed times now describe this concurrent workload. The five-round
collector still requires all reports, CRCs and 75 output files. Failures halt
indefinitely. Physical key latency remains explicitly NOT_TESTED: unattended
tests do not generate a physical keypress or measure the laptop display.

## Local validation

- All 22 host test groups and 35 hardware-tool unit tests passed.
- The final QEMU input run reported **zero dropped keys** and exact mixed-case,
  punctuation, editing and repeat text. Three confirmed active-write echo
  samples were **6.130, 17.796 and 15.867 ms**; output-load echo was **28.378 ms**.
  Idle samples ranged from 4.414 to 7.129 ms. These are emulator host-clock
  samples, not physical key-to-display measurements.
- Both 128 KiB competing writers, the 512 KiB readback, and the cancelled
  writer's complete 1 MiB output matched exactly. Cancellation returned 130.
  Independent `fsck.exfat -n` reported a clean volume (3 directories, 10 files).
- Evidence: `build/review/io-admission-final{.json,-serial.log,-qemu.log}` and
  `io-admission-input-stress.log`. The final `latency` output contains the
  zero-drop counter; it is checked separately from early-boot counters.
- The real USB volume passed an offline read-only check before staging:
  85 directories, 412 files, main VolumeFlags zero before and after, no repair.
  Evidence: `build/review/io-admission-usb-preflight.json`.

- The full five-round QEMU GRUB cycle passed with all five reports/CRCs and
  75 expected output files. Independent filesystem checking was clean
  (28 directories, 131 files). Successful runs reached the default boot entry;
  consumed-request replay and forced write failures remained halted. Evidence:
  `build/review/io-admission-five-round-cycle.log` and `io-admission-autotest-*`.

- The 481-command general shell regression passed, including process/job control,
  pipes, redirection, filesystem commands and the POSIX compatibility probe.
  Its final filesystem scan reported zero leaks and errors. Evidence:
  `build/review/io-admission-cli.log`.

- Workspace tests passed default read-only and marker guards, saved documents,
  history/scripts, sync, reboot persistence, write revocation and forced write
  failure. Independent checks were clean; prohibited/failed writes left their
  disposable disks unchanged. Evidence: `build/review/io-admission-workspace.log`
  and `io-admission-workspace-*`.
- The stalled USB read test passed bounded quarantine and keyboard progress,
  retaining the unchanged disk. Evidence: `build/review/io-admission-usb-timeout.log`.

## Physical acceptance pending

Launch a fresh five-round run using the exact locally tested payload hashes in
`build/review/clock-progress-tested-payload.json`. Accept it only after Ubuntu
collects all five PASS reports/CRCs and all 75 files, followed by a clean offline
check with clear main VolumeFlags and no repair. Preserve the tested rollback.
After acceptance, install a reusable workspace profile using those same tested
binaries and record the physical evidence here. Physical key-to-display latency,
hot-unplug and power-loss recovery remain outside this checkpoint.


## Physical failure and premature timeout correction

Source `13c02e6`, release `de38c6d228a1221c`, batch `f7f6257a240aeecf` did
not pass the physical checkpoint. Round one passed with all 15 files and report
CRC verified (shell 11.731 s, render 4,069 us). Round two `f7f6257a240aeece`
halted during file operations; its shell workload never started. Later requests
were untouched, and no complete report for round two survived.

The photographed finalization diagnostics identify WRITE(10) command 31174's
13-byte CSW read: pending=1, done=0, completion=0, actual=0. The xHCI reason was
`spin-budget`, after 20,000,000 polls and **291,399 us**. Thus the independent
iteration cap expired before the intended two-second bulk-transfer deadline.
This establishes premature timeout for this run; it does not prove when that
particular flash-drive operation would have completed, or identify every earlier
CSW failure.

The bulk loop now renews its fallback polling budget whenever the monotonic
clock advances. A progressing clock retains the existing two-second deadline;
a stopped/unavailable reference terminates with `clock-stalled`. The change adds
no retries and retains pending-DMA quarantine, error validation and failure halt.
Host regressions model a 500 ms completion requiring more than the old polling
budget, a genuine two-second deadline, and a frozen clock. All 22 host groups
passed. Keyboard and filesystem syscall admission are unchanged.

Evidence is retained in `build/hardware-tests/f7f6257a240aeecf/`: copied files and
requests, the photo, partial collection result, and an extent-hash-verified sparse
snapshot of the volume prefix and all 605 allocated 128 KiB clusters. Unallocated
cluster contents were not captured. Offline `fsck.exfat -n` found no structural
errors (93 directories, 443 files), with main VolumeFlags=2. After snapshotting,
`fsck.exfat -p` cleared the dirty flag; a second read-only check was clean with
flags=0. Round one's report and files revalidated after recovery. This recovery
does not turn the failed batch into a pass. A fresh physical batch is required.

The corrected build also passed the full five-round QEMU GRUB cycle, including
all reports/CRCs and 75 output files, independent clean filesystem checks,
default boot return, and indefinite failure halts. The stalled USB integration
test still passed bounded quarantine and keyboard progress. Evidence:
`build/review/clock-progress-five-round-cycle.log`, `clock-progress-autotest-*`,
`clock-progress-usb-timeout.log`, and `clock-progress-build.log`. Exact tested
payload hashes are recorded in `clock-progress-tested-payload.json`. Physical
acceptance remains pending a fresh five-round run.
