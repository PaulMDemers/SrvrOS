# MacBook Air processor sensors — 2026-10-18

Physical update: `b427b78f21644bd2` produced fresh CPU/package and fan readings
in the user's screenshots and saved session. The sensor screenshot shows 83 C,
1202 RPM and subsecond sample ages; logs reached 85 C with no SMC failures.
This confirms live telemetry on Air7,2, not calibration, idle-power behavior or
support for another model. See the [machine record](machines/macbookair7-2.md).

The opt-in `srvros.sensors=cpu` backend reads Intel digital temperature registers
on the initial target only: GenuineIntel family 6, model `0x3d`, with MSR and
DTS capability. Hypervisors are rejected. No fan controller, power limit, thermal
threshold or MSR is written. The v21 and current v22 desktop profiles enable the backend.

A dedicated worker samples the boot CPU's core and package every second and
publishes a versioned 72-byte snapshot. The System app's Sensors tab queries the
cache, reports sample age, and hides temperatures when older than three seconds.
The initial reference-temperature decoder accepts the observed zero-offset mode;
unsupported target settings and invalid core readings remain unavailable. Raw
registers are retained in the monitor/boot-session diagnostics. `thermal` prints
the current snapshot from the monitor.

The package register's validity semantics differ from the core register; the
core's bit 31 validity test is not applied to the package. Current thermal-limit
and critical flags are shown separately from historical sticky bits. The UI
identifies the core as the boot CPU: this is not an all-core sampler.

Read-only Ubuntu captures confirmed the CPU model and readable target/core/
package registers. Host tests cover that fixture, unsupported CPUs and virtual
machines, target offset rejection, invalid core readings, negative temperatures,
current versus sticky flags and UI stale-data behavior. The VM must display an
unavailable sensor provider. Native live readings were subsequently confirmed
in the physical update above.

Battery/AC remains unavailable. Neither this backend
nor a temperature reading establishes suspend/resume, thermal policy or fan
control support. The firmware's existing thermal mechanisms are unchanged.

Register definitions: Intel's [Software Developer's Manual, volume 4](https://cdrdv2-public.intel.com/671098/335592-sdm-vol-4.pdf),
architectural and family/model MSR tables. Implementation and tests are local;
no kernel driver was copied.


## Read-only SMC fan provider

`srvros.smc=read-only` adds fan telemetry in the v21/v22 desktop profiles. Binding
requires the captured DSDT's exact checksum/hash identity and the target GPU/LPC
IDs. Its APP0001 resource template declares ports `0x300..0x31f`, MMIO and IRQ 6;
this provider uses only the data and command/status ports, without MMIO or IRQs.

The transport accepts only key-read and key-metadata operations. It checks the
`FNum` and `F0Ac` lengths/types/readability before publishing the target's single
fan. The `fpe2` result retains quarter-RPM precision internally; the UI displays
RPM and sample age. Zero RPM is a valid measurement. Readings are not clamped to
advertised fan limits. Ubuntu's exported fan reading and descriptor evidence are
retained under `build/hardware/device-support-20261018-smc/`.

One I/O-service worker owns the protocol. Each poll has at most one status read
and one command/data access; the input-open and busy checks use separate reads.
Service slices are bounded, transactions time out, and uncertain completion
quarantines the provider. There is no speculative reset, extra-data draining,
key-write operation or fan-mode/speed command. Last-good samples retain their
timestamp and become unavailable on failure or age expiry. Diagnostic snapshots
record state, errors and sample counters every ten seconds during a trial.
Failures retain the queried fan key, transaction stage, byte counts and first
six response bytes; exit diagnostics repeat them for firmware mismatch diagnosis.

Host tests cover the target gate, metadata mismatch, unsupported fan count,
busy/extra/missing data, signed clock movement, cancellation, sample invalidation
and zero/maximum RPM decoding. The generic VM verifies an unavailable provider;
physical SMC sampling was subsequently confirmed in the v22 laptop round.

Protocol/register references: pinned [Linux Apple SMC](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/hwmon/applesmc.c)
and [QEMU Apple SMC](https://raw.githubusercontent.com/qemu/qemu/v9.2.0/hw/misc/applesmc.c).
The transport is an independent bounded state machine; no driver source was
copied. SMC command writes request reads; they do not change sensor/control keys.
