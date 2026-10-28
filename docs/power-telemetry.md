# Read-only CPU power telemetry

Status, 2026-10-19: **native read-only telemetry accepted on the reference Air7,2**
for the bounded desktop capture below. Interactive desktop profiles do not enable this provider.
Ordinary desktop staging does not enable it. This is measurement groundwork
before changing CPU frequency or idle-state policy.

## Scope and gate

`srvros.power=read-only` starts a one-second scheduled service. The monitor's
`power` command only reads its cached snapshot; it cannot enable the service or
write CPU controls. Workspace checkpoints save the same cached values, and
`hardware_desktop.py` includes them under `devices.power`. Disabled boots perform
no power MSR reads. Unsupported CPUs report unavailable without reading any MSR.

The profiles require GenuineIntel family 6/model 0x3d, 0x45 or 0x46, no hypervisor bit,
MSR support, CPUID.06H:ECX[0] feedback support and a working monotonic clock.
The base telemetry reads RAPL units (0x606), platform info (0xce), package energy (0x611),
APERF (0xe8) and MPERF (0xe7). The model-specific residency and capture-only
firmware audit extensions are described below. The observed energy exponent must be 14; the
platform non-turbo ratio must be nonzero and at most 64. No other CPU model is
implicitly enabled; CPU telemetry support does not accept a sibling Mac's other devices. There are no MSR writes,
ACPI mode changes, fan controls or power-limit changes.

The initial Linux capture on this Air confirms those five registers are readable,
units `0xa0e03`, and a nominal 2,200 MHz reference. It was taken during development
load; its derived 6,010 mW / 1,424 MHz interval is a decoder fixture, not a SrvrOS
idle result or an OS comparison.

## Meaning and validity

`package_uj` is energy consumed during the interval; `package_mw` is average
package power over that interval. This excludes the rest of the laptop and must
not be labeled battery power. `active_mhz` uses nominal MHz times the delta
APERF/MPERF ratio for the boot logical CPU while active. It is neither instantaneous
frequency nor frequency averaged over idle time or all cores.

State 0 is disabled, 1 unavailable, 2 sampling. Flag 1 validates energy/power;
flag 2 independently validates active frequency. Flag 4 marks a stale snapshot.
The first read is only a baseline, so it has no valid measurements. Zero MPERF
delta means frequency unavailable. An observed zero energy delta is retained as
zero, without claiming the sensor independently proves zero consumption.

Sampling accepts intervals from 100 ms through 5 seconds. Clock discontinuities,
long gaps, reserved energy bits and backwards performance counters invalidate
the affected measurements and rebaseline. A modulo-32-bit energy delta handles
ordinary rollover. A 100 W sanity bound rejects implausible energy jumps on this
mobile package; a 6 GHz bound rejects implausible frequency ratios. These are
validation ceilings, not hardware limits. Multiplication is bounded before use.
Neither a small counter reset near rollover nor sensor calibration can be proven
from two raw values alone. Values older than three seconds have their valid
flags and derived numbers cleared. Raw samples, time, interval and rejection
count remain visible. Collection flags stale/rejected samples for review.

## Validation and next physical trial

`make test-power` exercises actual driver sampling with injected CPUID/MSR reads,
unsupported profiles (zero reads), worker failure, cadence and cached expiry.
Arithmetic tests cover the captured Linux interval, energy rollover, resets,
time discontinuities, integer overflow, missing active counts and recovery.
`tools/power_smoke.py` verifies ordinary VM boot stays disabled and an explicitly
requested Broadwell VM refuses telemetry. VM refusal cannot validate native
energy readings. Host/VM evidence is recorded in the October 19 journal.

The immutable v26 hardware-stage profile adds `--power-telemetry` to
`--desktop-idle-capture --laptop-probe-volume <local-UUID>`. Exact-stage rehearsal
checks that the VM reports unavailable with zero samples, then completes the
normal desktop screenshot/log/save/return workflow. It does not fabricate power
values to make the VM pass. A missing workspace must still halt. Ordinary v25
and interactive profiles retain their original command lines.

The collector's optional `power_idle_sample` uses only valid cached checkpoints
inside the logged desktop measurement window. It requires at least three points
covering 20 seconds, validates counter continuity and rejects stale, conflicting,
reset or implausible records. It reports the actual covered time separately from
the full desktop window. This excludes the later screenshot write, whose USB
activity would otherwise distort an idle measurement. Unsupported or incomplete
samples yield no power result; completion of the overall diagnostic only accepts
the capture/save/return workflow.

For physical acceptance, retain the accepted recovery payload and collect a
bounded desktop interval with raw counters, a non-null covered power result and
a clean automatic return. Compare matched workload, display brightness, adapter
state, wall-clock duration and CPU0 idle counters; do not infer energy savings
from scheduler idle percentage. Frequency controls and deeper idle entry need
separate wake/input/storage regressions after a measured baseline exists.

## Accepted Air7,2 desktop baseline

Release `ed93e03b2585dcaf`, source `0f2038a` (same tree as integration
`1d44cee`), saved session `session-00065e38449c2146-00` on October 19.
Text was open without user interaction. The 60.003-second desktop window had
94.10% CPU0 idle samples, one presentation and 76,384 framebuffer bytes.
Six valid raw checkpoints covered 52.999 seconds inside that window:

| Measurement | Result |
| --- | --- |
| Package energy over covered interval | 342.298 J |
| Average package power | 6.458 W |
| CPU0 active-frequency ratio | 2,083 MHz |
| Rejected samples / collector attention | 0 / 0 |

The checkpoint values retained energy and frequency validity without stale
flags; raw counters advanced continuously. Package temperature fell from 68 to
66 C across those checkpoints. The saved 1440×900 screenshot shows the idle Text
window. Four logs and the BMP were copied and hash-verified; desktop completion
and clean process return were recorded. Ubuntu returned with the one-shot entry
consumed, and offline read-only exFAT checking was clean (165 directories,
827 files). Full evidence is retained locally outside `build`.

Linux preflight observed AC connected; SrvrOS retained its firmware backlight
level of 14%. Linux's pre-reboot brightness setting is not the native level.
There is no live SrvrOS AC/battery observation yet. This short post-boot sample
includes periodic device/USB logging and is not thermal equilibrium, whole-laptop
power, battery life or an energy-saving claim. The later screenshot write lies
outside the measured window. No CPU policy or native EC/SCI settings changed.

## Linux policy baseline

`tools/capture_linux_hardware.py` now saves `cpu-policy.json` alongside the
ordinary hardware inventory. It enumerates CPU and policy numbers rather than
assuming the Air's topology. It captures driver/governor names, policy membership,
frequency limits, CPU topology and idle-state attributes using read-only sysfs
access. No MSR device, input stream or power control is opened by this addition.

Frequency values retain kHz; transition latency uses ns. Idle exit latency,
target residency and accumulated time use microseconds. Idle counters describe
kernel requests/accounting, not independently measured hardware residency or
energy. `scaling_cur_freq` also need not equal instantaneous hardware frequency.
See the Linux [CPUIdle](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpuidle.html)
and [CPUFreq](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html)
documentation for these interfaces.

Values remain raw strings. An unsupported optional field is omitted; an observed
field that fails to read is null with an error in `capture.json`. Directory
availability is true, false for absent, or null for an enumeration failure. A
directory disappearing after enumeration is recorded as partial. CPU0 lacking
an `online` attribute is not interpreted as offline. Before/after online sets
can expose hotplug, but matching sets do not prove the snapshot was stable.
Start/end UTC and monotonic duration bound the observation, not a workload.

The reference Air's October 19 Linux snapshot completed in 33 ms with no partial
reads: four online logical CPUs, `intel_idle`/`menu`, and four `intel_cpufreq`
policies using `schedutil` with 500–3,200 MHz limits. CPU0 exposed POLL through
C10, all enabled in this snapshot. These are Linux settings, not new SrvrOS
capabilities or evidence that the hardware entered those states. Repeat the
capture for another machine or changed Linux configuration; never copy these
values into sibling profiles. Raw evidence is in
`build/hardware/20261019-cpu-policy/`.

## Primary references

- [Intel SDM Volume 4](https://cdrdv2-public.intel.com/929363/335592-093-sdm-vol-4.pdf),
  revision 093: §2.15 lists Broadwell 06_3D inherited register tables; Table 2-20
  provides platform ratio and package RAPL registers; Table 2-2 specifies the
  APERF/MPERF feature gate and paired reset behavior.
- [Intel SDM Volume 3B](https://cdrdv2-public.intel.com/929360/253669-093-sdm-vol-3b.pdf),
  §17.2 describes active-state performance feedback; §17.10 describes RAPL units,
  domain scope and the 32-bit package energy register.

Local source copies and raw Linux capture live in ignored
`build/overnight-20261018/`; no firmware dumps or personal inventory are committed.

## Pro11,2 CPU profile

The explicit 06_46 Haswell CPU profile is now admitted alongside 06_3D.
The Pro11,2 Linux capture has signature 0x40661, CPUID leaf 6 EAX=0x77/ECX=9,
units 0xa0e03 and platform 0x80838f3011900 (2,500 MHz nominal reference).
All five power registers and the three thermal registers were readable on CPU0.
The captured one-second development workload decodes to 33,173 mW package power
and 3,119 MHz active frequency. This is a decoder fixture, not an idle baseline,
whole-laptop power reading or native SrvrOS result. Temperature target is 100 C
with zero TCC offset. Other Haswell model IDs remain refused except the independently captured Air6,1 profile below.

The model identity and register family are cross-checked against the Linux
[Intel family definitions](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/intel-family.h)
and [RAPL driver](https://github.com/torvalds/linux/blob/v6.12/drivers/powercap/intel_rapl_common.c).
The local readable-register capture supplies this unit's actual constants.
Hypervisors, missing CPU features, unexpected energy units and invalid sample
intervals remain refused; no CPU control writes are added. Native Pro acceptance
requires its own complete v26 capture. The subsequent exact Pro two-fan profile
and paired native readout are now accepted in the Pro machine record.

Pro11,2 native v26 release `de341a3cfcaa7acb` now has a completed capture:
52.993 seconds of valid energy intervals inside a 59.992-second desktop window,
9.161 W package average and 3,466 MHz CPU0 active frequency. Scheduler idle
samples are 93.85%. Zero rejected power samples; thermal service is active with
fresh valid readings. Screenshot/save/unmount/return succeeded. This accepts
read-only telemetry on this unit, not thermal equilibrium, battery power, fan
support or a comparison against Linux's development-load fixture.

## Bounded load diagnostic preparation

`threadstress --cpu-load-capture` is an explicit, separate diagnostic, excluded
from the normal `threadstress all` suite. It performs 30 seconds idle, 30 seconds
single-worker integer load, then 30 seconds recovery. It logs monotonic phase
boundaries, completed work chunks, temperatures and paired fan snapshots. This
is neither a multi-core stress test nor a power measurement by itself. Package
energy needs the existing kernel telemetry and a separately reviewed collector
that delimits these phases; the idle collector must not label it idle data.

Both thermal and fan providers must be active, version-matched, valid and fresh
within three seconds. Sensor failure, thermal throttling/critical flags, a
regressing/stalled clock or sleep failure aborts with `completed=NO`. Load entry
requires core and package below 80 C. Every phase stops at the lower of 90 C
and ten degrees below the reported thermal reference. Fixed 16,384-iteration
work chunks keep clock checks outside the arithmetic loop; sensors are checked
at 100 ms intervals and logs emitted at one-second intervals. These diagnostic
thresholds are conservative test limits, not a hardware protection mechanism
or a replacement for firmware protection. CPU policy and fan control are unchanged.

Run host fixtures with `python3 tools/test_cpu_load_capture.py` and, after
building `all usb-image`, VM refusal checks with
`tools/host-env.sh python3 tools/cpu_load_smoke.py`. The VM checks disabled and
explicit-but-unavailable providers both refuse work and return to the monitor.
Host time/sensor fixtures exercise successful Air/Pro-shaped snapshots and
thermal, freshness, transport-validity and clock failure exits. They are not
native load acceptance. The integrated v35 profile below supplies automatic capture wiring.

The v35 profile stages with `hardware_boot.py stage
--laptop-probe-volume <local-UUID> --cpu-load-capture`. It enables only RAM/USB,
CPU/SMC readout and package telemetry, then runs the standalone diagnostic.
Handled refusal is saved and returns after clean unmount; missing workspace or
save/unmount failure halts. The process-return marker alone is not acceptance:
require the worker's `completed=yes`, ordered complete phase boundaries and
nonzero load work. `tools/hardware_cpu_load.py <exit.log>` parses the result and
computes energy only from fresh checkpoints wholly within each phase. At least
two checkpoints spanning ten seconds are required per phase; unavailable power
remains null. Console phase measurements are separate from the Text idle baseline.
Missing phase power remains incomplete telemetry evidence, even when the workload completes.

The Pro11,2 native v35 run `9b7ff9eb6ebc3535` completed all three phases.
Its covered load interval measured 27.190 W package power over 22.000 seconds,
and recovery 9.293 W over 21.000 seconds. Initial idle power remains unavailable
because eligible checkpoints cover less than ten seconds. Logged temperature
peaked at 81 C; no diagnostic stop occurred. See the machine record for the
session and limits; this is a short single-worker console result.

## Worker attribution for matched comparisons

The explicit read-only power capture now records a bounded CPU0 scheduler
snapshot at each device checkpoint. Each row carries a boot-lifetime identity,
reusable slot, copied name, timer samples, context switches into the thread,
deadline-expiry wakes and explicit wait-queue wakes. A wake is counted only when
a blocked thread becomes ready. An already-ready thread is not counted again;
slot reuse resets counters and receives a new identity. No userspace ABI or
scheduling deadline changes. Ordinary boots with power telemetry disabled do
not emit these additional records. Worker snapshot version 2 supports the
budgeted scheduler's slots 0..255; collectors retain version-1 parsing with its
original 32-slot bound. Missing/invalid versions and out-of-range slots still
refuse attribution.

`hardware_desktop.py` adds `desktop_worker_sample` to collection. It requires a
completed desktop capture and at least three complete, strictly ordered
snapshots covering 20 seconds within its measurement window. Missing rows,
duplicate identities/slots, counter regressions and changed identity bindings
refuse attribution. Only lifetimes present throughout that covered interval
receive deltas; partial lifetimes are listed separately. Later screenshot work
is excluded. Keep the full raw log; a missing summary is incomplete evidence.

These are software transitions, **not hardware wakeups, elapsed CPU runtime or
energy**. Timer sampling can miss short workers. The measurement logger itself
uses CPU and USB and appears in the results. Compare candidates with the same
instrumentation, logging cadence, workload, panel setting, AC and adapter states.
The legacy v26 capture excludes the ordinary Home battery owner; its result
must not be labeled whole-desktop battery-service efficiency. A fresh v26
baseline is the first bounded attribution step, followed by matched repeated
captures before claiming savings. No CPU policy or deeper-idle change is made.

### October 23 worker-attributed Air baseline

Release `dc7c0eaea0dca69b` (`7e0d8b9`) completed the v26 capture with six valid
energy checkpoints spanning 50.999 seconds inside 59.999 seconds of Text idle:
341.451 J, 6.695 W package average, 2,064 MHz CPU0 active frequency. Scheduler idle
samples were 97.60%. Six worker snapshots spanned 51.269 seconds: compositor,
xHCI input and one client each had roughly 100 deadline wakes per second;
network had 95.65 deadline plus 1.97 signal wakes per second. No partial lifetimes,
rejected energy samples or collector attention. The screenshot and all original
files are preserved; offline allocation and recovery pass.

Native brightness was 14%, AC was connected before boot, the USB Ethernet and
workspace were attached, and audio was idle. The ordinary battery owner was off.
Package temperature fell 74–71 C during the covered interval. This is accepted
measurement evidence, not thermal equilibrium, whole-laptop power or savings
against the older October 19 capture. Repeat matched controls before claiming
an improvement. See the machine record for the exact evidence/session.

### First follow-up: idle Text queue wait

Implementation `d6f3136` adds the [client wait contract](native-desktop.md#quiescent-client-message-waits).
The exact v26 VM captures reduce the idle Text worker from 98.31 to 9.96 context
switches per second; typing and exact save after idle, screenshot persistence and
automatic return pass. Other device polling is unchanged. This is a measured
software-activity reduction, not a power result. Native release
`7afccbd6fbc3cfd4` completed the same unattended capture in
`session-00065e8b0600c122-00`. Text switches fell from 100.00 to 10.01/second
(510 over 50.960 seconds), about 90%; total 60-second-window switches fell
30,487 to 25,102 (17.66%). Idle timer samples rose 97.60% to 97.85%. Other input
and compositor polling remained 100 Hz. USB preservation, screenshot, offline
allocation, clean exit and Ubuntu recovery all passed.

Package average was **6.802 W**, compared with baseline 6.695 W: this pair does
not demonstrate energy savings. Six candidate checkpoints covered 51.000 seconds,
346.927 J and 2,054 MHz CPU0 active frequency. Package temperature fell 75–72 C
versus baseline 74–71 C; neither interval was thermally settled. Device settings
and diagnostic recipe matched, including the disabled ordinary battery owner.
Accept reduced idle software activity only. Physical typing after idle remains
untested by this unattended capture; VM typing/save is separate evidence.
The next optimization needs preserved input/IO behavior and measured activity;
repeat thermally controlled comparisons before making a power claim.


### Compositor follow-up: polling reduction, overall efficiency unresolved

PR112 adds a [combined server wait](native-desktop.md#quiescent-compositor-waits)
without changing hardware polling. Source `92edfcb`, v26 release
`dc06bc673689e011`, passes VM post-idle titlebar drag and typing/exact save,
context menus/clipboard, input during a guarded 4 MiB+17-byte copy, clean capture
and missing-workspace recovery. The exact EFI/GPT VM records 9.946 compositor
switches/second; power remains unavailable in VM.

The matching Air7,2 session `session-00065e8b5e2897aa-00` returned automatically,
with complete capture, clean exit, reviewed screenshot, all prior files preserved
and clean offline filesystem/allocation/recovery checks. The native compositor
records **540 switches over 53.920 seconds (10.015/second)**, down from 100.00.
Text stays at 10.015/second; hardware input polling stays at 100/second.

| Measurement | Original baseline | Text wait | Compositor wait |
| --- | ---: | ---: | ---: |
| Idle window (seconds) | 59.999 | 59.999 | 60.010 |
| Total scheduler switches | 30,487 | 25,102 | 19,278 |
| CPU0 idle timer samples | 97.60% | 97.85% | **92.88%** |
| Logger timer samples / worker interval | 127 / 51.269 s | 96 / 50.960 s | **392 / 53.920 s** |
| Covered package average | 6.695 W | 6.802 W | **6.946 W** |
| Covered energy interval | 50.999 s | 51.000 s | 54.000 s |
| Package temperature over energy interval | 74–71 C | 75–72 C | 76–73 C |

The additional reduction in total switches is about 23.2%, but overall idle
samples worsened and package power was 144 mW higher. **No overall CPU efficiency
or energy saving is established.** These non-equilibrium single runs do not
isolate causality. The logger accounts for most additional busy samples;
its path writes, reads back and flushes each snapshot, and USB completion waits
poll while periodically yielding. Existing counters cannot separate waiting
from filesystem work. Measure checkpoint duration and transfer wait, and repeat
matched captures before changing that path or assigning a cause.

Candidate covered energy is 375.135 J, active CPU0 frequency 2,086 MHz. AC was
connected before boot, brightness 14%, Ethernet and workspace attached, audio
idle. This recipe omits the ordinary battery owner and does not exercise physical
input after idle or ordinary Home battery-service efficiency. Accept the bounded
polling reduction and persistence; keep those other acceptance questions open.

### Logger cost records

With a reference clock available, the workspace logger measures each existing
snapshot save independently of optional CPU power telemetry. No MSR service is
enabled by these clock measurements. Earlier payloads required power telemetry
to be on; absence of their receipts is not evidence of a fast save. The original
`workspace-cost: version=1` reports start/end, bytes and preparation/write/
verification/sync microseconds, plus USB BOT callback counts and elapsed time
for CBW, data and CSW phases. The current version 3 retains these intervals and
adds the transfer/service-gap detail below. Timing changes no transfer, timeout,
cadence, validation or flush policy.
The generic BOT wrapper is measured, so no controller-specific instrumentation
is required. Failed callbacks are counted; regressing/overflowed clocks invalidate
the record. A nested measurement scope refuses without resetting its owner.

Wall time includes scheduler service, event dispatch and copies; USB totals are
system-wide during that save, including other traffic if it runs. They are not
CPU runtime or pure device latency. Sync may also flush other open files. Do not
subtract these values and call the remainder CPU work. The probes themselves add
clock-read and log-formatting overhead; compare instrumented runs with each other.

The proposed logger-only committed-data barrier flushes the completed direct
exFAT log mutation and its device, without submitting unrelated dirty process
file buffers. Explicit workspace sync, unmount and scratch preparation keep full
process-file sync. `workspace-log: durable snapshot=... monotonic_us=...` identifies
the exact successfully persisted snapshot; it does not promise durability for
another application's pending buffers. New collectors require that matching
receipt; historical payloads retain their older workspace-flushed receipt.
This change is under review alongside the overlapping filesystem audit.

A completed record is emitted before the **next** snapshot is copied, with no
extra write. Thus exit.log contains earlier periodic operations, not its own
write/flush measurement. The desktop collector exposes `desktop_logger_cost`
only for valid, successful, nonoverlapping periodic records wholly inside a
completed idle window. Straddling/startup/after-idle operations are excluded;
missing, malformed, conflicting or failed in-window evidence yields null. Totals
cover the recorded saves only, not the full idle interval or lost tail records.
This supplies attribution evidence; it does not change the unresolved efficiency
conclusion above. Native acceptance is bounded to the named capture below.

#### First native logger timing capture

Air7,2 release `9c57ca38a3771eef`, source `1f93899`, session
`session-00065e8bb03c4641-00` passed automatic return, complete capture, screenshot,
file preservation, offline filesystem/allocation and recovery checks. Six
completed periodic records inside the 60.009-second idle window cover 136,678
snapshot bytes and **3.005369 seconds** of save wall time.

| Logger stage | Recorded total |
| --- | ---: |
| Preparation | 0.325 ms |
| Write | 2,985.075 ms |
| Verification | 14.184 ms |
| Sync | 5.785 ms |

USB callback time is nested within these stages: CBW 4.554 ms, data 546.660 ms,
CSW **2,437.047 ms** (691 CBW/CSW calls, 2,200 data calls, no callback failures).
The callback sums cover 99.43% of save wall time, and status callbacks 81.09%.
One save lasted 2.002016 seconds, with 1.938771 seconds across 108 status calls;
others lasted 0.149–0.343 seconds. An aggregate two-second save is not a single
transfer timeout. These measurements locate time in transport callbacks but do
not isolate media, firmware, scheduling or the longest individual request.

The whole idle window recorded 95.00% idle timer samples, 19,401 switches and one
frame. The logger had 282 samples over the separate 52.819-second worker interval;
those samples are not measured runtime. Package average was **6.949 W** over
52.999 seconds (368.323 J), versus 6.946 W in the uninstrumented predecessor.
Temperature fell 76–73 C. No energy saving is established; comparison includes
probe overhead and unsolved run-to-run variation. Native physical input after
idle and ordinary Home battery efficiency remain separate acceptance tasks.

Next investigate bounded USB-completion waiting and individual request costs,
while preserving input service, deadlines, DMA quarantine, verification and
flush policy. Avoid attributing this cost to rendering or reducing persistence
merely to improve the idle score.

### Bounded storage completion wait

The refined storage bulk policy polls directly for the first twenty milliseconds, retaining
its one-millisecond service yields. Thereafter it may block on its per-device
completion queue until a matching event or the next timer tick. The existing
HID/network worker can drain events and wake the owner; a one-tick retry also
allows progress when no input worker is polling. The queue is woken only after
completion code/residue/done state has been published for the matching slot,
endpoint and TRB. Wrong and stale events do not complete the transfer.

The scheduler helper requires an active I/O owner in its recorded address space,
enabled services and a running/reference-ready timer. It refuses deadlines more
than one tick ahead; readiness and expiry are rechecked with IRQs masked before
blocking. It retains the I/O gate, excluding unaudited filesystem work and the
owner's sibling user threads while eligible services, other audited processes
and idle can run. It returns with IRQs masked. Refusal falls back to the existing
yield/poll loop. Normal scheduler/timer delivery is required for timed blocking.

The original two-second reference-clock deadline and stopped-clock spin budget
are not renewed by a sleep. A failed transfer still quarantines its pinned DMA
buffer/rings with no retry or reuse. This policy changes neither flushing nor
verification nor controller interrupt routing. Waits can add up to one timer
tick of polling latency (plus scheduling delay) to a slow completion; validate
throughput as well as CPU samples. Device callback wall time includes sleeping
and must not be relabeled CPU work. Native bounded-workload evidence is recorded below; energy savings remain unproven.

The `xhci` storage status includes cumulative `wait_calls` and `wait_us` for
accepted helper calls. They include a call that discovers completion before
blocking and yielded service time; they are not a CPU sleep-time counter. The
USB large-copy VM fixture requires nonzero calls and more than 10 ms of observed
helper wall time, in addition to input overlap and exact persisted bytes.

The first native candidate `58a61b892c43d2f5` completed capture, persistence and
recovery with no USB callback failures. Idle samples improved (95.00% to 98.77%),
but five saves took 5.889351 seconds versus six baseline saves taking 3.005369
seconds. The records contain different byte totals and coverage. Completion
polling latency and media variability were not yet separated at that checkpoint.
The refined 20 ms follow-up below completes bounded-workload validation. See the reference
[machine record](machines/macbookair7-2.md) for exact observations and limits.

### Controlled USB wait comparison

`tools/hardware_boot.py stage --laptop-probe-volume <local-UUID>
--storage-wait-capture` selects the separate
`ram-usb-v1-storage-wait-laptop-probe-<UUID>-auto-return` recipe. It does not alter
historical idle-capture recipes or ordinary startup. The diagnostic pauses
periodic logging, creates a new 32 KiB `wait-fixture.bin` inside its collision-safe
session directory, and performs 18 overwrite/readback/flush measurements. All
six permutations of polling, 4 ms and 20 ms poll-before-wait thresholds run once,
so each policy appears twice in each position. Allocation is outside measurement;
metadata writes, verification, flush and eligible service time remain included.

Each row records callback costs by USB phase, elapsed time and CPU0 timer samples.
There is a 180-second admission limit checked between measurements, not a promise
to interrupt a filesystem operation. Existing per-transfer deadlines and DMA
quarantine remain authoritative. Restore the entry policy on handled completion;
a quarantined transfer refuses policy mutation and makes the run incomplete.
No retry or DMA reuse is introduced. The diagnostic stops on its first error.

The collector requires all 18 ordered successful rows, policy restoration and
exact offline payload bytes. Missing, duplicate or malformed rows, unexpected
paths, symlinks, changed fixture bytes and missing exit logs do not pass. Host
injection covers timer quantization separately from physical media variability.
VM timings validate the workflow only. Native results must precede selecting a
new default policy; neither CPU timer samples nor callback wall time alone prove
power savings or interactive responsiveness.

The native fixed-file comparison `2218ef4113730414` passed all 18 measurements
and exact offline readback. Median operation times were 251.303 ms polling,
172.592 ms with 4 ms polling and 171.813 ms with 20 ms polling. Stalls appeared
under all policies, with unequal incidence. This single run does not prove an
optimal threshold or rank energy efficiency. Choose 20 ms conservatively to
avoid tick-quantization penalties on common short completions, while still
allowing longer stalls to sleep. The normal desktop logging follow-up below
validates this default for the bounded workload. The diagnostic parser accepts entry policies
of 4 ms (historical capture) or 20 ms, and requires restoration in either case.


### Individual USB transfer timing

Initial individual timing uses `workspace-cost: version=2` and the fixed-file
comparison uses `storage-wait: begin version=2`; the latest service-gap format is
version 3, described below. Each save/sample has three `usb-timing`
phase records keyed by its monotonic start: phase 0 is CBW, 1 is data chunks and
2 is CSW. Historical version-1 results remain readable without inventing missing
measurements. Version 2 requires all three records and exact agreement with the
aggregate callback counts/times; absent, malformed or inconsistent detail fails
collection.

For each phase the diagnostic records callback maximum, counts strictly above
20 ms / 100 ms / one second, and counts/totals/maxima for two software intervals:

- `observe_us`: immediately before the doorbell to dispatch of the matching
  slot/DCI/TRB completion event. This includes device/USB latency **and any delay
  servicing the event ring**; it is not a hardware completion timestamp.
- `resume_us`: that observation to the synchronous storage caller recognizing
  completion. This includes rescheduling and intervening bookkeeping. It does
  not measure time asleep or CPU execution.

Interval maxima are independent; they need not describe the same request. The
sum of both interval totals cannot exceed callback wall time. An observed error
completion can carry timestamps, but a timeout has no invented completion. Only
a successful, completely observed capture is accepted by the collector. Backward
clocks, impossible order and counter overflow invalidate telemetry. Failed I/O
still fails independently of telemetry. No trace allocation, extra transfers or
console output occurs inside a request; three clock reads are added only while
the timing scope is active. Aggregates are printed after the scope ends and
persisted in a subsequent snapshot, so a save never claims its own duration.

Directed host fixtures inject delayed event delivery and a separate known caller
resumption delay, verify stale/duplicate event exclusion, retain the original
bounded timeout/quarantine checks, and reject missing or inconsistent persisted
records. These measurements do not change the 20 ms normal wait threshold,
verification or flush policy.

Air7,2 release `b824d64a95b27a3b` completed all 18 native operations and exact
readback. All 1043 prior files retain their bytes except expected exFAT metadata;
offline allocation and recovery checks pass. Across 2124 CSW callbacks, total
wall time was 5121771 us: 5117657 us preceded software event observation, and
1616 us followed it. The maximum callback was 307216 us; maximum post-observation
caller delay was 20 us. Seventeen CSW callbacks exceeded 100 ms, none one second.
The 2427278 us operation included twelve separate slow CSW callbacks, consistent
with the earlier multi-second aggregate durations without a single transfer
crossing its two-second deadline. Two data callbacks also exceeded 100 ms.

This narrows attribution to the interval before event observation for this run.
Controller/media latency and delayed event-ring servicing remain unresolved.
The six operations per policy had unequal slow-transfer incidence (3 / 2 / 12
CSW callbacks over 100 ms for polling / 4 ms / 20 ms); a single run cannot rank
policy performance. Keep the 20 ms default. The next diagnostic should bound
pending-transfer event-service gaps before changing transport or scheduler policy.
Normal-desktop input and package energy were not measured by this fixed fixture.

### Pending-transfer event-service gap bounds

Logger and fixed-file comparison version 3 require `usb-timing: version=2`
phase records. Older logger/comparison versions 1 and 2 remain readable, without
inventing these new fields. For each phase, `service_checks` counts actual calls
to inspect the event ring while a timed storage request is pending;
`service_samples` counts timestamped checks (the first and every 256 thereafter).
`service_gap_max_us` is the maximum interval between submission, those sampled
checks and matching-event dispatch. `completion_gap_max_us` is the maximum final
interval from the last sampled check through matching-event dispatch. These
maxima can describe different requests.

Sampling makes the reported maximum a conservative bound: omitted checks can
only shorten the actual intervals between ring inspections. Include the initial
and final intervals so a servicing hiatus cannot disappear at either boundary.
The input worker and the synchronous caller share the actual `poll_event` hook;
checks by either count. A matching completion closes the sample before waking
the caller. Timeout clears the diagnostic owner without fabricating a completion
or releasing DMA. Clock rollback, counter overflow, absent check coverage or
impossible aggregate bounds invalidate collection. A request's sampled checks
must equal `ceil(checks/256)`; phase totals retain the corresponding bounds.

No extra polling or wakeups are introduced. An active timing scope adds a clock
read at most every 256 ring checks; ordinary untimed I/O omits these samples.
The original wait threshold, two-second deadline, stopped-clock budget, input
service, quarantine, verification and flush policy remain in place. Host tests
inject steady servicing with a 500 ms delayed completion, a separate 75 ms
servicing pause, an independent caller-resumption delay, rollback and saturation.
Version-3 collectors refuse older or missing detail instead of calling it zero.

These bounds measure how often software inspects the ring, not the controller's
completion time. Even a small bound cannot establish when the target event was
posted, exclude a backlog of other events, identify a device fault, or rank wait
policies.

Air7,2 release `46b9132589405099` (compiled `00ad3cc`) completed all eighteen
native measurements, exact offline readback and automatic return. All 1046 prior
files retain their bytes except expected exFAT metadata; offline allocation and
recovery pass. The maximum CSW callback was 281928 us. The slowest whole operation,
5724579 us, included 33 CSW callbacks above 100 ms under continuous polling;
286274049 ring checks / 1118319 timestamped samples bounded its maximum service
gap to 13 us. Waiting modes recorded maximum sampled gaps of 10011 us, consistent
with their existing tick-bounded service. Across 2124 CSW callbacks, total
callback wall time was 10108415 us: 10103993 us before software observation and
1403 us after it, with post-observation caller delay at most 3 us. No single
transfer exceeded one second; five data callbacks also exceeded 100 ms.

Thus long lapses between ring checks do not explain the observed long callbacks
in this run. Controller/media behavior and event backlog remain possible. Slow
callback incidence differed between policies (33 / 2 / 12 CSW callbacks over
100 ms for polling / 4 ms / 20 ms), so one run cannot rank policy performance or
energy. Keep the 20 ms default, deadlines, DMA quarantine, input service,
verification and flush policy. The bounded diagnostic milestone is complete;
further attribution can wait for a reproducible workload regression. This fixed
fixture supplies no native interactive or normal-desktop logger acceptance.

### Refined default: native normal desktop result

Release `f340e0695d1e2492` (compiled `c258822`) completed the one-minute Text-open
capture and returned automatically. CPU0 idle timer samples were 98.30%, versus
95.00% in the polling baseline. Six saves / 136710 bytes took 1.883555 seconds,
versus six / 136678 bytes in 3.005369 seconds; individual saves ranged from
149.316 to 622.689 ms. Logger samples were 84 over 51.529 seconds, versus 282
over 52.819 seconds. Compositor activity remained about 10 switches/second.
There were zero USB callback failures, exact preservation of all prior files,
clean offline allocation and verified Ubuntu recovery.

This supports the 20 ms default for the tested storage workload, alongside the
counterbalanced comparison and slow USB copy/input VM regression. It does not
establish optimality or physical interactive acceptance. Package power measured
6.786 W over 51.999 seconds versus baseline 6.949 W, but this thermally unmatched
single pair does not establish savings. Ordinary Home battery efficiency remains
a separate study; this capture deliberately kept the battery service off.


### Network notification readiness

The selected provider may advertise a side-effect-free `rx_wake_ready` callback.
It is true only when receive notifications and transmit servicing can progress
independently of the network worker: e1000 requires initialized rings and enabled
IRQs; ECM requires a ready device, operational controller, running xHCI service
and no input probe. Absent/false callbacks retain one-tick polling. With readiness,
the empty receive worker waits on its existing notification-generation predicate
for at most 50 ticks (the existing recovery period). TCP timer work is unchanged.
Missed notification delivery still reaches a recovery poll; this is not a new
controller interrupt path and does not slow the USB input service.

ECM notification endpoints can send repeated link/speed reports without packets.
The USB service already consumes these independently; only a link state change
or fault wakes stack waiters. Receive completions still signal immediately.
Initial QEMU evidence found roughly 33 stack wakeups/second with repeated reports
before this filtering, despite zero packets. Retain that failed idle-ceiling run;
it is separate from the refined candidate's acceptance. Full functional traffic,
cable recovery, fallback and idle measurements precede a native capture. No
energy reduction or physical responsiveness is assumed from fewer switches.


Use `tools/hardware_boot_smoke.py --probe-check --probe-network` with the v26
stage to attach an ECM adapter to the exact EFI/GPT rehearsal and require a
network worker below ten switches/second over at least twenty seconds. The
ordinary probe fixture has no supported adapter and cannot validate network
activity. Missing-workspace return remains a separate case. Traffic and
post-idle input checks use the dedicated network/idle fixtures.


Air7,2 native release `27b239f96a183fb8` completed the unattended one-minute
capture with 102 network worker switches over 47.728 seconds (2.137/s), versus
5005 over 51.529 seconds (97.13/s) in `f340e0695d1e2492`. xHCI input service
remained 100 switches/s and the compositor about 10/s. Five completed saves,
the desktop screenshot, exact prior-file preservation, clean offline allocation
and automatic Ubuntu recovery passed. This accepts reduced redundant network
worker activity for the bounded workload, not native traffic/input or energy.

Do not infer a broader performance improvement: CPU0 idle samples were 97.52%
versus 98.30%, package power 6.840 W versus 6.786 W, and five saves took 7.942433 s
versus six in 1.883555 s. Candidate saves ranged 200.222–3792.337 ms with zero
callback failures; 7.531335 s was in CSW callbacks, including scheduling time.
The cause of this run's slower USB saves is unresolved. Preserve the evidence
for controlled storage timing follow-up. At that checkpoint the legacy TCP readiness fixture also failed on both kernels.
The subsequent [TCP readiness follow-up](tcp-readiness.md) fixes passive-open
queueing and e1000 receive ownership, with directed host/VM coverage. That fix
does not explain USB save variability or establish energy savings.


## Air6,1 CPU profile and automated trial

The independently captured 06_45 Haswell-U profile admits signature `0x40651`
on this MacBookAir6,1. CPU0 reports CPUID.1 ECX/EDX `0x7ffafbbf`/`0xbfebfbff`,
CPUID.6 ECX `9`, units `0xa0e03` and platform `0x8083df3011400`.
The platform register supplies the **2,000 MHz** reference ratio; do not replace
it with the CPU brand string's 1.40 GHz. The captured Linux development-load
interval decodes to 6,610 mW package power and 834 MHz active frequency. This is
a decoder regression fixture, not a native idle measurement. Intel SDM Volume 4
lists 06_45 among the supported package RAPL register families. The existing
five-register read-only allowlist and every validity/refusal bound remain intact.

Use the v35 command above with this unit's own USB UUID after the normal build,
exact-stage VM, recovery and filesystem preflight. Leave AC and USB connected;
no input is required. The console displays phase progress, then saves and
returns to Ubuntu. Require all three phases, nonzero load work, valid power
coverage in each phase, preserved files and clean offline allocation checks.
An emulated refusal verifies save/return only. Native Air6,1 acceptance is pending;
ordinary desktop power telemetry, CPU policy and fan control remain unchanged.

The first Air6,1 v35 run (`22632f396dab67a4`, October 27) completed all three
phases with fresh valid power counters and zero rejected samples. Covered load
measured 5.134 W / 1,300 MHz over 10.999826 seconds; recovery measured
4.127 W / 1,288 MHz over 20.999667 seconds. Initial idle remains unavailable:
its two eligible checkpoints span 9.999841 seconds, below the ten-second floor.
The next checkpoint's logging timestamp falls outside idle. This is incomplete
three-phase evidence, not a reason to relax coverage or claim an idle baseline.
The next capture needs more reliable phase checkpoint coverage. Peak logged
package temperature was 63 C, and automatic return/file preservation passed.

### Bounded phase checkpoint retention

The explicit v35 CPU-load path now starts a bounded in-memory power capture
around `threadstress --cpu-load-capture`. After an ordinary power sample, the
provider retains its cached snapshot when at least five seconds have elapsed
since the previous retained point. Up to 24 snapshots fit in static storage;
there are no additional MSR reads, allocation, disk writes or sample wakeups.
Ordinary power/desktop boots do not activate this capture. The bootstrap caller
stops retention before emitting the records and the final workspace save.

`cpu-load-power` rows carry the original sampling timestamp as `monotonic_us`;
they are historical checkpoints printed after the phases, not current readings
at print time. The collector verifies the count/overflow receipt, combines these
with ordinary workspace checkpoints, and retains the same per-phase boundaries,
ten-second minimum, freshness, counter continuity and validity checks. Missing,
truncated or overflowed captures cannot supply phase power. The 24-row bound is
not extended if a diagnostic stalls. The ordinary ten-second USB logging cadence,
30/30/30-second phases and all thermal stops remain unchanged. This instrumentation
changes the capture baseline; do not infer savings by comparing it to an older run.

### Accepted Air6,1 idle/load/recovery capture

Release `a5f8fdf5d377c116` (`e7a41714`, PR210), session
`session-00065ed4d4558b8e-00`, completed the automatic 30/30/30-second trial on
October 27. Seventeen retained checkpoints had valid energy/frequency flags,
zero rejected samples and no overflow. Combining them with ordinary workspace
checkpoints gives 20.000003 seconds of idle coverage (4.128 W / 1,288 MHz),
25.000003 seconds of load (5.161 W / 1,299 MHz), and 25.000004 seconds of recovery
(4.107 W / 1,290 MHz). All satisfy the unchanged ten-second minimum.

The single-worker load completed 250,682 chunks; peak logged package temperature
was 66 C without a diagnostic stop. The final report, automatic Ubuntu return,
prior-file preservation and offline allocation checks passed. This accepts the
bounded read-only telemetry and checkpoint capture on this Air6,1, not CPU/fan
controls, equilibrium or whole-machine power, or an energy-saving comparison.
Ordinary staging still leaves power telemetry disabled.

## Automatic Air6,1 desktop phases

The explicit `--desktop-power-test --desktop-battery --battery-air61
--desktop-services --desktop-volume <local-UUID>` staging recipe selects v78.
It keeps the ordinary Home filesystem and battery-owned desktop lifetime, enables
read-only power/temperature telemetry, and runs without input: wait for a rendered
Text window and fresh battery data, settle for 30 seconds, idle for 60 seconds,
cycle Text/Files/System applications for 60 seconds, then recover for 60 seconds.
The last 12 seconds of the app phase drain pending work. At least six complete
app cycles and a newer fresh battery generation in every phase are required.
The desktop displays progress once per second; it is part of this measured load.
Audio is initialized but no sound is played. Keep AC and the workspace connected.

The existing 500 ms desktop loop and 10 s app lifecycle limits stay unchanged.
A phase may finish up to five seconds late, but this does not relax loop or app
limits. The diagnostic has a 300 s owner watchdog and uses normal lifetime
revocation, battery retirement, log save and workspace unmount before reboot.
Uncertain ownership retains the existing fail-closed behavior.

Only this diagnostic retains up to 32 ten-second log checkpoints, in distinct
`checkpoint-00.log` through `checkpoint-1f.log` files. It reserves 64 file slots,
never wraps, and refuses further saves if that bound is reached. Ordinary logs
continue rotating two snapshots. Write/readback verification, sync and logging
cadence are unchanged. `tools/desktop_power_records.py` merges overlapping saved
snapshots without repairing partial records and requires ordered phase receipts,
clean battery retirement, at least three power/worker checkpoints covering 20 s
inside each phase, and fresh valid temperature readings. Report actual covered
intervals, package power, CPU0 active frequency, temperature ranges and per-worker
software timeout/signal wakeups for stable worker lifetimes, with partial
lifetimes listed separately. These are not whole-machine power, hardware wakeup
counts or a power-policy acceptance. No requirement asserts that app workload
must consume more energy than idle; this experiment measures that difference.

`tools/desktop_power_smoke.py` checks a complete synthetic battery VM lifetime,
checkpoint coverage, file preservation and offline allocation. With `--stage
<release>`, it boots the exact native payload and requires Q35 profile refusal
and clean unmount/return. The harness records its explicit `--accel tcg|kvm` selection. Failed rehearsals
remain failed evidence; neither mode changes guest limits. Synthetic execution never supplies native power or
temperature acceptance. Native results remain pending until matching saved
sessions and recovery evidence have been collected.

## Accepted Air6,1 combined desktop phase baseline

On October 27, release `1d1cbd1cb03d3032` (runtime `7a9cfeaf`, PR212) completed
native session `session-00065ed629188a57-00`: 30 s settling followed by roughly
60 s each of idle, 13 Text/Files/System cycles and recovery, with continuous
battery monitoring and normal retirement/Ubuntu return. The saved records pass
the unchanged power validity, 20 s covered-interval and UI/app timing checks.

| Phase | Covered power interval | CPU package mean | CPU0 active frequency |
| --- | --- | --- | --- |
| Idle | 50.998362 s, 6 points | 4.131 W | 1,292 MHz |
| Apps | 51.998342 s, 6 points | 4.150 W | 1,294 MHz |
| Recovery | 40.998694 s, 5 points | 4.137 W | 1,292 MHz |

Core/package temperatures stayed within 57–59 C during these phases. The maximum
UI loop was 115.440 ms, app readiness 704.385 ms and close 51.434 ms. Worker
coverage is independently bounded to 40–52 s per phase; short-lived app workers
remain explicitly partial. Idle timeout wakeups include about 90/s for `xhci-hid`,
89/s for `uthread` identity 17, 48/s for `bootstrap` and 19/s for `battery-owner`.
Those counters justify a polling audit, not a claim about which worker consumes
energy. This single short workload does not establish significant power changes
or accept CPU/fan controls. See the [unit record](machines/macbookair6-1.md) for
phase temperatures, retirement, file preservation and durable evidence details.


## Accepted Air6,1 event-driven worker comparison

PR214 release `cb881058052a1012` / runtime `e0008d92` repeats the same native v78
recipe in session `session-00065ed6efff4c7c-00`. All phases, 13 app cycles,
battery retirement, file preservation and offline allocation pass under unchanged
requirements. Lifecycle `uthread` timeout wakes fall from approximately 89/s to
zero; covered idle/recovery intervals also contain no lifecycle switches. App
work produces 36 signal wakes in 40.829 s. Supervisor and device polling remain
unchanged; software wake counters still do not attribute energy consumption.

Mean CPU package power is 4.121 / 4.167 / 4.130 W for idle/apps/recovery versus
4.131 / 4.150 / 4.137 W previously. Coverage spans 41–52 s with 5–6 checkpoints
per phase; CPU0 active frequency stays around 1,293 MHz. Core/package readings
span 60–65 C, warmer than the prior run. This accepts the targeted wait behavior,
not significant power savings, hardware residency or CPU policy. The
[unit record](machines/macbookair6-1.md) retains the full bounded comparison.


The Air6,1 supervisor candidate (`aa576838270d623a`) also replaces ordinary desktop
supervisor polling with process/control/ownership events and original absolute
deadlines. The power VM shows zero bootstrap timer wakes in every phase and no
bootstrap switches during covered idle/recovery. Native comparison against
`cb881058052a1012` is recorded below. Battery sampling cadence, service watchdogs,
firmware guards, ownership requirements and the v78 recipe remain unchanged.


The supervisor candidate is now physically accepted on this Air6,1 in session
`session-00065ed778e804ca-00` (release `aa576838270d623a`, PR216). All three phases,
13 app cycles, battery retirement, file preservation and allocation checks pass.
Bootstrap records zero timer wakes throughout and zero switches/signals during
covered idle/recovery intervals; app work causes 25 event wakes. The previously
accepted lifecycle-worker reduction is retained. Worst UI loop is 110.061 ms.

Package means are 4.130 / 4.140 / 4.132 W across idle/apps/recovery, versus
4.121 / 4.167 / 4.130 W previously, with 41–51 s power coverage and 5–6 points
per phase. These short observations establish reduced supervisor polling, not a
meaningful energy saving. Battery-owner cadence, USB HID, firmware watchdogs,
ownership validation and test limits are unchanged. See the
[unit record](machines/macbookair6-1.md) for bounded measurements and recovery.

## Air6,1 read-only hardware idle residency

Explicit `srvros.power=read-only` now also samples hardware idle counters on
captured native GenuineIntel family 6 model 45, with invariant TSC indicated by
CPUID.80000007:EDX[8]. All existing power feature/hypervisor gates apply first.
The later [Air profile parity work](air-profile-parity.md#broadwell-read-only-counter-contract)
adds a separate seven-counter Broadwell profile; other admitted power profiles
keep their original reads and report no residency.
This adds no policy writes, provider activation or sampling wakeups. CPU policy,
HLT/MWAIT selection, fan control, thermal stops and battery watchdogs are unchanged.

The captured allowlist is CPU0 core C3/C6/C7 (`0x3fc`–`0x3fe`) and package
C2/C3/C6/C7 (`0x60d`, `0x3f8`–`0x3fa`), C8/C9/C10 (`0x630`–`0x632`). Intel
[SDM Volume 4](https://cdrdv2-public.intel.com/774500/335592-sdm-vol-4.pdf), tables
2-20 and 2-31, specifies TSC-rate counters; C8–C10 use 60 bits, the others 64.
Read-only Linux CPU0 capture on this unit confirms each register is accessible.
That is inventory evidence, not SrvrOS acceptance or a comparable idle baseline.

Each existing one-second sample adds ten residency reads bracketed by two reads
of the TSC MSR (`0x10`). Snapshot ratios use the current end minus previous begin
TSC bracket, conservatively including read skew. Units are basis points (100 =
1%). Core and package totals are checked independently; neither is a per-thread
percentage. Do not infer C0/C1 or whole-machine idle from their complement.
Hardware C-state names are not ACPI/MWAIT request numbers. No control MSRs are written. The explicit firmware audit below separately reads
E2 at its capture boundaries. There is no cross-core sampling or all-core average.

First samples, missing support, stale data and rejected discontinuities have no
valid residency mask. Zero counts with full validity are legitimate zero
residency. Backwards/reset counters, invalid reserved bits, invalid TSC ordering,
arithmetic overflow, state totals exceeding their TSC interval and the existing
100 ms–5 s sample interval constraints invalidate/rebaseline the sample. They are
never clamped or treated as wrapped idle counters. Residency rejection is separate
from energy/frequency validity and cumulative rejection evidence is retained.

The unchanged v35 test runs 30 seconds each idle/load/recovery, retaining at most
24 cached records at intervals of at least five seconds. After the phases,
`cpu-load-idle` rows accompany the retained power rows with identical original
sample timestamps. Each row contains support/valid masks, rejected count, TSC
brackets, raw counters and one-sample ratios. Indices 0–9 follow the allowlist order
above. The collector requires the complete bounded receipt, exact power-row
pairing, valid monotonic data and at least ten seconds of coverage in each phase;
it independently calculates phase ratios from raw endpoints. Missing evidence is
unavailable, never zero or a pass. The native run requires both power and residency
coverage, workload completion, preserved prior files and clean offline allocation.
No minimum deep-state residency or energy-saving result is required. The returned
Air6,1 result is recorded below.

### Native Air6,1 residency result (2026-10-27)

Release `ed06103a5275d7ca` (PR218 runtime `d7cda917`) completed the unchanged
30-second idle/load/recovery phases and returned automatically. Fifteen retained
records have full residency validity, zero rejection/overflow and five records
covering about 24 seconds per phase. All ten raw residency counters remained
exactly zero while TSC advanced: no measured core C3/C6/C7 or package C2–C10
entry during this run. Zero is valid data here, not missing support. C0/C1 and
other physical cores are not measured, so this is not evidence that the CPU
never idled. The bounded telemetry path is accepted; deep-state entry and CPU
policy remain unaccepted.

Covered package averages were 4.130/5.167/4.137 W with active CPU0 frequencies
1,287/1,299/1,290 MHz for idle/load/recovery. Power and residency intervals differ
because power also uses ordinary checkpoints. Peak logged temperature was 67 C.
Files were preserved, offline allocation was clean and the full required log
fit within the unchanged 64 KiB tail. See the [unit record](machines/macbookair6-1.md)
for exact intervals and evidence identity. Next audit idle entry and firmware/
package constraints without changing policy; these results alone do not identify
the cause of absent deeper residency or establish potential energy savings.


## Capture-only idle-entry and firmware audit

On captured native 06_45, the explicit bounded power capture now counts CPU0
scheduler idle attempts immediately before the unchanged `sti; hlt; cli`, returns
after that sequence, and readiness-check bypasses that yield to existing work.
Accounting is disabled outside the capture. It adds no clocks, allocations,
printing, polling or I/O to the idle path. Start refuses nesting or an absent
idle worker. Stop freezes the counters. A generation token prevents an old
preempted HLT return from being attributed to a restarted capture. An outstanding
return at a sample/stop boundary is allowed; counts do not measure time asleep,
interrupt counts or hardware C-state residency.

A source audit finds no MONITOR/MWAIT instruction or Limine MP request/AP startup
in the current kernel. The managed scheduler is CPU0-only. CPUID.1 EBX is retained
as capability/topology evidence, **not** a count of running processors. Other
cores' live state is unknown. CPUID.1 MONITOR support and raw CPUID.5 registers
are retained only where advertised; there is no MWAIT activation or hint choice.

The only new MSR is a read of `MSR_PKG_CST_CONFIG_CONTROL` (`0xe2`) at capture
start and stop, under the existing native 06_45/invariant-TSC gate. Ordinary power
sampling adds no configuration reads. Linux CPU0 inventory confirms this register
is readable; observed `1032` (`0x408`) decodes to package limit C10, unlocked,
I/O-MWAIT redirection enabled and no demotion bits. Linux can configure this
register, so this is not a claim about its value under SrvrOS. Raw native values
are required before drawing a firmware-restriction conclusion.

For 06_45, Intel SDM Volume 4 table 2-31 defines bits 3:0 as package-limit
encodings C0/C1, C2, C3, C6, C7, C7s, C8, C9, C10 (0 through 8), bit 10 as I/O
MWAIT redirection, bit 15 as the configuration lock and bits 25–28 as C3/C1
demotion/undemotion flags. Unknown limit encodings remain unknown. A permitted
package limit does not prove software requests, core/device conditions or actual
entry. These bits are observed, never changed. No ACPI methods or control I/O
are added; power policies, worker sampling/logging cadence, thermal stops,
watchdogs and every existing timing/capacity/integrity bound remain unchanged.

After measured phases, `cpu-idle-audit` records both raw configuration values,
capabilities and final counters; `cpu-idle-entry` records pair one-for-one with
retained residency timestamps. Strict collection requires a unique complete
header, matching generation/count/timestamps, monotonic counters with at most one
outstanding return, and the existing ten-second phase coverage. A configuration
change is explicitly reported, not normalized away. Valid zero entry counts are
diagnostic evidence, not a fabricated failure/pass for deep-state support.
The native trial also requires all existing power/residency, completion, file
preservation and offline allocation proofs. Native acceptance is recorded below.


### Native idle-entry/firmware audit result (2026-10-27)

Air6,1 release `6b1f017332e0931b`, session `session-00065ed949956fde-00`, completed
all three unchanged 30-second phases and returned automatically. Paired retained
coverage gives 2,378 HLT entries/returns over 23.999235 seconds idle, zero over
22.999267 seconds load, and 2,371 over 23.999234 seconds recovery. All ten raw
core/package deeper-state counters remain zero. This establishes that the existing
HLT path is reached; it does not measure C0/C1 or time asleep.

E2 is unchanged at `0x408` at capture start/stop: the package-limit field permits
C10 and is unlocked, with I/O-MWAIT redirection enabled. That field alone does
not explain the absence of deeper residency. MONITOR capability is advertised,
but the current kernel uses HLT and other cores' live state remains unknown.
No firmware, scheduler power policy, cadence or limits changed. Package power
averages 4.147/5.174/4.139 W across covered idle/load/recovery intervals; these
short measurements do not establish savings or battery-life behavior.

All coverage/completion, prior-file preservation and offline filesystem checks
pass. The 65,290-byte exit log fits the unchanged 64 KiB tail with little spare
space; account for that before adding diagnostics. See the
[unit record](machines/macbookair6-1.md#idle-entryfirmware-audit--bounded-native-acceptance-2026-10-27)
for exact counts, intervals, evidence identity and remaining work. Boot-time
secondary-core state and prerequisites for a separately authorized deeper-idle
experiment remain open.

## CPU0 topology and deeper-idle prerequisites

The follow-up audit stays on the captured native 06_45/invariant-TSC path. It
reads `IA32_APIC_BASE` (`0x1b`) and `IA32_MISC_ENABLE` (`0x1a0`) at explicit
capture start/stop only, alongside E2. Intel SDM Volume 4 table 2-2 defines the
BSP/APIC-mode flags and MONITOR FSM enable bit 18. No MSR is written. Three fixed
CPUID.0B subleaves (0, 1, 2) record the SMT/core/terminator topology when the
maximum basic leaf permits it; no unsupported-leaf alias is queried. Ordinary
power sampling/status adds no reads, workers or wakeups.

One compact `cpu-idle-prereq` receipt carries the capture epoch, maximum leaf,
three EAX/EBX/ECX/EDX tuples and start/end APIC/misc pairs. Strict collection
requires exact fields, widths, bounds and a matching complete idle audit. It
preserves configuration changes and unknown topology layouts explicitly. A
recognized Haswell topology gives addressable logical CPUs/core/package, not
online CPUs. CPUID.5 extension/break flags and substate nibbles are capabilities,
not a chosen MWAIT hint or proof of entry. The existing 24-record, ten-second
coverage and 64 KiB saved-log bounds are unchanged.

This unit's Linux read-only inventory gives leaf-B tuples `1,2,256,0`,
`4,4,513,0`, `0,0,2,0`: two addressable threads per core and four per package.
Captured MADT lists enabled APIC IDs 0,2,1,3 and four disabled placeholders.
These firmware flags do not report live AP execution. Linux reads APIC base
`4276096256` and misc `8716425`; Linux's configuration is not native acceptance.

The pinned boot binary is Limine v11.4.1. Its corresponding
[source](https://github.com/limine-bootloader/limine/blob/v11.4.1/common/protos/limine.c)
checks for an MP request before calling `init_smp`; SrvrOS has no such request.
This establishes that the kernel does not request that AP startup path, not
whether Apple's firmware leaves each AP halted, in wait-for-SIPI or another
state. UEFI's [x64 environment](https://uefi.org/specs/UEFI/2.10/02_Overview.html#x64-platforms)
specifies a uniprocessor boot-services environment; it is not per-AP telemetry.
CPU0 CPUID/MSR reads cannot resolve remote cores' live state. Doing so would
need an independently designed AP rendezvous/ownership experiment; none is
added or silently accepted here.

Before a future deeper-idle experiment, establish an exact-model MWAIT hint,
MONITOR support/enabling, monitored-memory ownership and race-safe readiness
recheck, interrupt wake behavior, scheduler cancellation and fallback. Review
timer behavior and bounded wake latency without relaxing deadlines, plus sibling
thread/package/device prerequisites. [Intel's MWAIT reference](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf)
requires MONITOR and describes the capability-dependent interrupt-break option.
This audit does not execute either instruction, start APs, send IPIs, evaluate
AML, enable tickless operation or change CPU/fan/battery policy. Native receipt
acceptance is recorded below.


### Native CPU0 prerequisite result (2026-10-27)

Air6,1 release `8022333a9240598e`, session `session-00065edabaa7e840-00`, completed
the unchanged idle/load/recovery test with complete prerequisite and phase proof.
Native leaf-B reports APIC ID 0, two addressable threads/core and four/package;
maximum basic leaf is 13. APIC base is `4276095232` at both boundaries (BSP=1,
APIC enabled, x2APIC disabled), while misc is `8716425` (MONITOR FSM enabled).
CPUID MONITOR advertisement agrees; CPUID.5 reports extensions and masked-interrupt
break support. These are native prerequisite observations, not validation of an
unimplemented MONITOR/MWAIT path or live remote-core state.

E2 remains `0x408`, with an unlocked C10 package limit. HLT entries continue in
idle/recovery, while all ten deeper-residency counters remain zero. Covered
package averages are 4.150/5.144/4.141 W. Policies and limits are unchanged; a
separate opt-in wakeup/latency experiment with HLT fallback is the next proposed
step, not an accepted policy change. AP rendezvous remains separate. See the
[unit record](machines/macbookair6-1.md#secondary-coreprerequisite-audit--bounded-native-acceptance-2026-10-27)
for exact intervals, file integrity and evidence identity. The 65,484-byte saved
log contains the required proof but leaves almost no margin under the existing
tail limit; budget or compact any future diagnostics without loosening that limit.

## Explicit shallow MONITOR/MWAIT wakeup trial

`stage --laptop-probe-volume <local-UUID> --cpu-load-capture --mwait-wakeup-test`
selects the separate `ram-usb-v1-mwait-laptop-probe-*` profile. It adds
`srvros.idle=mwait-wakeup-test` only to the explicit CPU-load capture. Ordinary
boot, desktop and v35 captures retain HLT. Admission requires the existing native
06_45/invariant-TSC power gate, a live idle audit/reference clock, 64-byte minimum
and maximum monitor lines, CPUID MONITOR plus extension/masked-interrupt-break
support, a supported C1 substate, enabled MONITOR FSM and BSP xAPIC mode matching
the captured unit. Refusal emits `admitted=0 fallback=hlt` without either new
instruction; it is not native wakeup acceptance.

Only the idle worker tries MONITOR/MWAIT. A private, static, 64-byte-aligned WB
kernel line is separate from writable diagnostic counters. Interrupts are masked,
MONITOR arms that line, then the existing ready predicate is checked again. Ready
work bypasses MWAIT. Otherwise EAX=0 requests C1 substate 0 and ECX=1 permits a
pending maskable interrupt to break the wait even with IF clear. The unchanged
100 Hz interrupt supplies wakeups; there is no AP startup, IPI, firmware method
or MSR write. This is an interrupt-wakeup experiment, not validation of a remote
store-signalling protocol. [Intel's instruction reference](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf)
defines the hint and interrupt-break capability. A timer interrupt is still a
hardware prerequisite: software cannot enforce a timeout while the CPU itself
is stuck in an instruction.

The PM reference clock brackets each wait before pending IRQ dispatch, so the
reported maximum is masked wait duration, not IRQ dispatch, runnable latency or
time in a hardware C-state. A return exceeding the existing two-tick scheduler
quantum (20 ms), or a regressing clock, latches fallback to the original
`sti; hlt; cli`. After 32 attempts, HLT is mandatory even on success. No trial
state is accessed after enabling IRQ dispatch; epoch checks prevent a retired
capture from issuing more waits. The default path adds no clocks or new polling.
No deeper-state hint or permanent idle policy is introduced.

One bounded `cpu-mwait` receipt reports attempts/returns, maximum interval,
fault, post-trial HLT entries, readiness rechecks and timer progress. Acceptance
requires all 32 waits/returns, nonzero maximum at most 20 ms, advancing ticks
within attempts, 32 subsequent HLT entries and no fault, plus unchanged native
prerequisite configuration and the complete existing power/residency/idle-entry
phase proof. Missing, refused, faulted and incomplete trials are not passes.
The outer completion result also fails an unsuccessful wakeup trial. Existing
30-second phases, ten-second coverage, thermal/fan limits, watchdogs, 24-record
capacity, 64 KiB tail and filesystem checks are unchanged. Exact-stage VM tests
exercise unsupported refusal/save/return and missing-workspace safe halt; they
do not accept native instructions or wakeup latency. Native results follow below.


### Native shallow wakeup result (2026-10-27)

Air6,1 release `8030ae20c2611a09`, source `94c2b9e7` (PR224), session
`session-00065edb20755615-00`, passes the bounded shallow trial: 32 attempts and
returns, 9,971 us maximum masked wait, zero faults, advancing timer ticks and
32 subsequent HLT entries. All three 30-second phases and existing coverage,
thermal/fan, file-preservation and offline-allocation requirements pass, with
automatic Ubuntu return. Native configuration snapshots are unchanged.

The maximum is wait duration before IRQ dispatch, not scheduling latency. No
readiness bypass occurred in this native run; host tests cover that race. The
trial finishes before retained phase intervals, so package averages of
4.122/5.167/4.127 W predominantly describe HLT fallback. All ten deeper counters
remain zero. This accepts shallow interrupt wakeup and fallback only; deeper
entry, other-core live state, energy savings and permanent idle policy remain
open. See the [unit result](machines/macbookair6-1.md#shallow-monitormwait-wakeup--bounded-native-acceptance-2026-10-27).

## Explicit bounded C3 hint trial

`stage --laptop-probe-volume <local-UUID> --cpu-load-capture --mwait-deep-test`
selects `ram-usb-v1-mwait-c3-laptop-probe-*` and `srvros.idle=mwait-c3-test`.
It is mutually exclusive with the shallow trial. Normal boot, desktop and prior
capture profiles are unchanged. The exact native 06_45/invariant-TSC gate is
retained, with the same MONITOR line/extension/BSP checks. Additional admission
requires the CPUID.5 slot for hint `0x10`, an E2 package limit permitting C3,
CPUID.6 EAX.ARAT, and a live calibrated periodic LAPIC timer at the existing
100 Hz target, vector 32 and divide-by-16 setting. Missing prerequisites refuse
before MONITOR/MWAIT. No timer mode, register policy or limit is changed.

The [Linux v6.12 Haswell table](https://github.com/torvalds/linux/blob/v6.12/drivers/idle/intel_idle.c)
identifies hint `0x10` as C3; its CPU-online path requires timer broadcast when
ARAT is absent. SrvrOS instead refuses this experiment without ARAT and its
existing calibrated timer. This is an exact-model selector, not a generic ACPI
C-state encoding. CPUID substate availability alone does not identify a state's
physical name or prove entry. Firmware and sibling-thread constraints may
prevent a requested hint from reaching core or package C3.

The existing race-safe private MONITOR line, IRQ-masked readiness recheck,
32-attempt cap, 20 ms PM-clock interval check, epoch retirement and mandatory
32-entry HLT fallback are reused. Hint `0x20` and all other new selectors are
refused. No AP startup, IPI, new AML, MSR write or automatic deeper policy exists.
Software still cannot enforce a timeout while a CPU is stuck inside MWAIT;
ARAT/live timer admission and physical results are prerequisites, not a promise
of an independent hardware watchdog.

Only deep attempts additionally read TSC around core C3 (`0x3fc`) and package
C3 (`0x3f8`) counter endpoints. Reads bracket each wait before IRQ dispatch;
the sums exclude later HLT and phase work. Counter regression, impossible
counter delta above its TSC bracket, or accumulator overflow latches fault 3
and HLT fallback. No wrap is inferred. The unchanged wait-clock faults remain
1 (regression) and 2 (over-limit). Ordinary and shallow paths make no new MSR
reads. Broader brackets imply lower-bound residency ratios, not exact sleep
time or IRQ/runnable latency. Shared-core/package counters do not identify
another logical processor's live state.

Version-2 `cpu-mwait` identifies hint 16. One compact `cpu-mwait-deep` receipt
records ARAT, final timer readiness, aggregate TSC/core-C3/package-C3 ticks.
Collector acceptance still requires every existing wakeup/phase/integrity check.
Valid zero residency remains an explicit unobserved state, not proof of deeper
entry; wakeup PASS and `core_c3_observed`/`package_c3_observed` are separate.
A failed trial invalidates its residency inference. The same three 30-second
phases, ten-second coverage, thermal/fan stops, watchdogs, ownership checks,
24-record capacity and 64 KiB tail remain in force. Exact-stage VM refusal tests
cannot accept native deeper wakeup or residency. Native results follow below.


### Native C3-hint result: bounded wakeup passes, residency unobserved

Air6,1 release `980f4936c6282637`, source `2a8fabd0` (PR226), session
`session-00065edb80a92930-00`, records 32 hint-16 waits/returns, maximum masked
wait 9,968 us, zero faults, timer progress and 32 HLT fallback entries. ARAT and
live timer readiness pass. All three phases, coverage, thermal/fan, file and
allocation requirements pass with automatic Ubuntu recovery.

The separate per-attempt bracket totals 620,687,304 TSC ticks but **zero core
C3 and package C3 ticks**. Thus the bounded hint/wakeup/fallback path is accepted;
actual C3 residency is not. All deeper counters also remain zero in retained
phase samples. Package averages of 4.100/5.120/4.101 W mostly measure the later
HLT fallback and do not establish MWAIT power savings. Firmware configuration
is unchanged and remote logical-processor state remains unknown. Investigate
missing residency before claiming a deeper idle policy. See the
[unit result](machines/macbookair6-1.md#bounded-c3-hint--native-result-residency-unobserved-2026-10-27).


The same unit's read-only Ubuntu reference confirms working deeper residency
counters with the same package-limit value. The next prerequisite is the separate
[AP ownership diagnostic](ap-ownership-diagnostic.md): bounded private AP
rendezvous and retirement before any coordinated C3 experiment. Air6,1 release
`53b2c4c90f65e647` now accepts 96 native mailbox exchanges across three APs,
maximum measured acknowledgement 2 us, zero faults and terminal retirement under
the unchanged 20 ms bound. Ordinary boot requests and power policy remain unchanged.
This provides no C3, physical C1 residency or energy-saving acceptance; see the
[unit result](machines/macbookair6-1.md#ap-ownership--native-acceptance-2026-10-28).

The separate [coordinated C3 candidate](ap-ownership-diagnostic.md#coordinated-c3-candidate)
now pairs that ownership path with the existing bounded BSP trial. Per-round
peer readiness, wake acknowledgements and individual residency brackets retain
the 32-attempt/20 ms limits and HLT fallback. Host/VM validation does not accept
native coordinated entry; ordinary startup and power-policy registers remain
unchanged. See the Air6,1 record for the exact candidate and subsequent evidence.


Air6,1 release `57274dfaccfdc0b9` now accepts that bounded coordinated core-C3
experiment: all 32 rounds observed entry on both physical cores, maximum whole
round 9.983 ms, maximum BSP wait 9.951 ms, 32 HLT fallback receipts and clean AP
retirement. Package C3 remained valid zero. The short measured BSP wait brackets
were 99.686% core C3; this is not a whole-session idle ratio or energy saving.
The later 30-second phase coverage retains approximately 4.05/5.13/4.09 W package
power with C1 parking after the trial. Ordinary startup/policy remain unchanged;
package blockers and sustained operation remain open. See the [unit acceptance](machines/macbookair6-1.md#coordinated-c3--native-acceptance-2026-10-28).

### Identifying long serialized I/O owners

Workspace logging enables a kernel-only bounded I/O-owner observation history.
Each outer scope captures a stable scheduler thread identity, process ID (zero
for kernel workers), sanitized copied worker name and monotonic start time.
Nested scopes belong to that same owner; foreign begin/end attempts do not
create or finish a record. The history retains the last eight completed scopes
lasting at least 100 ms. Scope sequence numbers include shorter scopes, so gaps
are expected. Snapshot headers report total scopes, qualifying scopes, retained
and overwritten counts, clock regressions and the threshold. A footer brackets
the copied snapshot. Repeated snapshots may repeat a scope sequence number.
Validate the saved atomic log records: live UART output may interleave with other
writers and does not provide equivalent record completeness.

`io-owner` intervals are wall time while serialization is held, including device
waits and time spent servicing other eligible threads. `exclusions` counts READY
thread selection refusals during that scope, not unique threads, milliseconds or
CPU work. Correlate intervals with app phase and USB receipts; a long overlapping
scope alone does not identify every device command or prove CPU starvation.
Missing/overwritten history cannot prove that no other owner contributed.

Reset refuses an active gate or unavailable reference clock. Recording neither
allocates nor prints, and does not change admission, fairness, wakeups or gates.
Workspace checkpoints copy and print the bounded history outside I/O ownership;
there is no new user syscall or wire ABI. Ordinary boots without workspace
logging do not enable it. Host fixtures cover nesting, foreign refusal, copied
identities, clock rollback, bounded overwrite and output outside ownership.
Native attribution and full endurance remain pending until matching evidence is
collected.
