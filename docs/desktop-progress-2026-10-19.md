# Desktop reliability and idle work — 2026-10-19

Work starts from shared baseline `3b0b43a` on
`device/macbookair7-2/reliability-input-power`. Physical observations below remain
separate from host/VM results. Normal startup continues to exclude optional AML
probing. The accepted installed desktop is still `b427b78f21644bd2`.

## Physical secondary click

The trackpad now chooses the button at the initial physical press: one contact
means primary, two contacts mean secondary. That identity stays latched through
contact changes until physical release. This preserves the established gesture
of holding one finger down and adding another to steer a primary drag. Palm and
cancel suppression require release before rearming. Physical clicks are independent
of tap preferences; Settings explicitly labels the two-finger option as a tap.

Validation: `test-pointer` and `test-xhci` pass, including raw Type 3 reports with
bulk I/O pending, contact-count transitions, cancellation and no duplicate tap.
`all test-desktop` and the context-menu VM pass. The VM verifies secondary-button
routing and menus, not physical finger recognition. Native acceptance remains
pending the next human test.

## Input delivery timeout

The compositor previously checked queued-input age before attempting delivery.
A delay in the compositor could therefore close a healthy app without testing
whether it could accept the next event. Delivery is now attempted first; the
timeout starts with an actual failed send and resets on successful progress.
Timeout logs include blocked duration, event type and frame-progress age.

A regression reproduces a server delay longer than the old deadline, recovery
after IPC backpressure, ordered delivery, and bounded closure for a client which
continues refusing input. `all test-desktop`, context-menu and System VM checks
pass. This fixes a demonstrated timeout error but does not prove it caused the
System closure in the previous physical session. Frame/configuration lifecycle
deadlines remain in place; native follow-up is required.

## Portable machine inventory

The Linux collector no longer selects only `05ac:0291`. It enumerates Apple HID
report descriptors across product IDs without capturing input events or HID
unique strings. A structured topology file covers board model identifiers, DRM
cards/connectors/modes/raw EDID, PCI identities/driver bindings and block-device
ancestry. This supports comparing sibling Macs without widening driver gates.

Four host tests pass, including multiple Apple products, unsupported vendors,
missing reports, Retina-sized modes, storage partitions and excluded control/
serial properties. A read-only privileged capture on this Air7,2 completed with
zero missing reads. It confirms two Apple HID descriptors (26 and 182 bytes),
Intel `8086:1626` as boot VGA and the connected 1440x900 eDP panel. Raw capture
remains under ignored `build/overnight-20261018/linux-inventory`; no sibling
machine has been tested by this change.

## Idle scheduling and console input under storage load

Measurements found a demonstration worker running continuously, a busy USB
poll loop and a sleep syscall which merely yielded while remaining runnable.
The scheduler now has a fallback-only idle worker using an interrupt-safe HLT
sequence. It never takes a runnable worker's turn. Sleeping applications block
until their deadline or a signal; USB polling waits one timer tick (nominally
10 ms), while the storage transfer path still services HID completions directly.
Network polling wakes on receive notifications with a one-tick fallback.

Before these changes, disposable QEMU runs used approximately 100% of one host
CPU at both console and desktop idle. Initial comparisons used 7–22% at the
console and 21–33% at desktop idle, depending on network configuration. These
are host CPU observations, not physical power/temperature measurements. The
`workers` command and periodic USB logs now retain scheduler samples and switch
counts. Timer sampling is coarse and can miss short runnable work.

The heavy typing regression exposed a second issue: linenoise uses `poll()` for
escape continuations, and even console polling waited behind owned filesystem
I/O. Two failing runs retained keys in the kernel while the shell waited at the
I/O gate. Poll now copies/validates its bounded request before classification,
admits console input only, and rechecks descriptor bindings after waits. Other
descriptors retain the gate. Console readiness reflects buffered keyboard/serial
input, and input arrival wakes poll waiters. A bare Escape can therefore time
out instead of blocking on an unconditional readable result.

Validation so far:

- `all test-core`, scheduler/sleep/poll race and admission host checks pass.
- USB ECM passes DHCP, ICMP, 160 UDP echoes, TCP, keyboard overlap and disconnect.
- System with e1000 networking and the complete threadstress suite pass.
- Desktop audio during USB writes verifies 810,997 PCM frames exactly, zero
  underruns, input-file bytes, stop/cancel and subsequent tone playback.
- The repaired USB latency workload retains exact mixed-case/punctuation/edit/
  repeat text, concurrent file contents and cancellation cleanup; exFAT is clean.
  Key-to-serial echo samples were 8–25 ms idle, 5–16 ms during writes and 32 ms
  during output. These are VM host-wall-clock samples, not physical key latency.

Native automated acceptance: release `54a944ae1f7849ac`, source `686edd8`,
completed all five filesystem/shell rounds and returned to Ubuntu. Collection
validated each completed report and retained file set; an unmounted read-only
exFAT check was clean (159 directories, 799 files). Render elapsed time was
4,161–4,260 us for 1,000 lines; shell rounds took 11.2–14.1 seconds. Physical
key latency, gestures, sound and idle temperature remain untested by this batch.
The previous boot entry retains accepted desktop `b427b78f21644bd2`. Raw
comparison, failure diagnostics and repaired-run evidence are under
`build/overnight-20261018/`.

## Bounded idle capture

A separate v23 diagnostic profile samples 60 seconds of console idle with the
v22 device services and periodic USB logging. The bootstrap thread blocks until
the deadline. Cached sensor and scheduler snapshots are preserved every ten
seconds; completion records elapsed time and scheduler deltas. Only successful
sampling, log save/flush and unmount permit automatic return to Ubuntu. Missing
workspace or failed completion stays halted. AML remains disabled.

The staged VM verified the full measurement, multiple durable checkpoints, a
clean exFAT image and automatic reset; a separate missing-drive run stayed
halted. The collector exposes a validated measurement from `exit.log` without
calling it physical power or desktop acceptance. Installer tests also cover the
separate profile and normal hyphenated exFAT UUID arguments. Physical sampling
is the next step; the five-round suite above did not measure idle power.
