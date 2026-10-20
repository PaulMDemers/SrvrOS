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

The profiles require GenuineIntel family 6/model 0x3d or 0x46, no hypervisor bit,
MSR support, CPUID.06H:ECX[0] feedback support and a working monotonic clock.
It reads only RAPL units (0x606), platform info (0xce), package energy (0x611),
APERF (0xe8) and MPERF (0xe7). The observed energy exponent must be 14; the
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
with zero TCC offset. Other Haswell model IDs remain refused.

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
native load acceptance. Automatic physical capture/profile wiring is pending
shared boot-path coordination with PR6; no native load trial is armed.
