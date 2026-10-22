# EC SMBus event ownership study

Status: VM-tested study (`1797e8a`); separate native capture implemented,
physical acceptance pending. Ordinary native query10 remains refused. This extends the
[native AML audit](battery-native-aml-audit.md) and existing cached battery
service. It does not enable mounted Home coexistence or ordinary startup.

## Ownership choice

The captured Air7,2 namespace identifies an EC SMBus host controller with
`_HID=ACPI0001` and `_EC=0x2010`. Its query10 AML has different behavior depending
on shared OSYS: one branch notifies the host controller, the other processes an
alarm under the firmware mutex and changes private battery state. A snapshot
of OSYS cannot select a stable branch.

The explicit VM command `acpiruntime battery-smbus-study` instead claims the
host-controller event in the OS. It never executes or rewrites `_Q10`, spoofs
OSYS, or synthesizes an AML battery notification. Both synthetic OS branches
contain a forbidden operation in `_Q10`, so accidental dispatch fails the test.
The native backend cannot grant the VM study capability. A distinct explicit
parked capture capability is described below. Its existing predispatch
classifier and parked refusal capture remain unchanged.

ACPI defines query delivery after protocol completion, separate completion and
alarm status, and alarm fields held until acknowledgement. The event handler
copies the address and both data bytes before writing zero to status. Completion
alone needs no write. See [ACPI 6.4 §12.9](https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/12_ACPI_Embedded_Controller_Interface_Specification/smbus-host-controller-interface-via-embedded-controller.html).
Linux's host-controller driver also registers an OS handler for the query from
`_EC`; this is a useful architecture reference, not imported code. See
[Linux sbshc.c](https://github.com/torvalds/linux/blob/master/drivers/acpi/sbshc.c).

## Bounded protocol

The shared helper requires exclusive EC and bank ownership for the entire
operation. The runtime uses the existing single worker and refuses entry while
a sampler, transaction or lease is active. It validates the host identity, bank,
query, SBS interface and absence of a requested Global Lock before enabling the
study. Those checks do not replace the native firmware/resource profile.

1. Require an idle protocol and valid status; errors and reserved bits fail.
2. For completion or a spurious no-alarm event, return without writing.
3. For an alarm, copy its shifted address and two data bytes. Reject malformed
   addresses. Record unknown valid senders without inventing battery meaning.
4. Recheck idle protocol and unchanged status before the sole zero-status write.
5. Mark acknowledgement complete only after the EC write handshake finishes.

The helper has a one-second overall deadline and bounded individual EC transfers.
Clock rollback, busy/raced banks, partial reads and uncertain writes poison the
transport. There is no blind retry or successful retirement claim after failure.
A terminal VM fault keeps the state available for diagnosis.

Known SBS sender addresses 9–11 invalidate the cached sample and request another
sample from the service owner. The worker never samples recursively. A generation
barrier prevents publication of a sample completed before that alarm; unknown
senders are recorded and acknowledged without a battery refresh. The event's
raw payload stays in diagnostics; firmware-private BSSW is not updated.

## Validation and remaining acceptance

Host tests exhaust all status and raw address bytes, transfer stalls, bank races,
clock rollback and deadlines. The VM harness is
`tools/battery_smbus_smoke.py`; it uses disposable Q35 firmware and no host disk.
It checks completion-only events, both OS branches, unknown senders, busy/error
status, partial reads, uncertain acknowledgements and invalid descriptors.
The later-alarm case checks cached generation 2 becoming nonfresh and being
replaced by fresh generation 3. Native-command and ordinary-dispatch refusal are
separate regressions. All 13 cases passed on `1797e8a`, along with both diagnostic
builds, the full host core suite and proof-parser corruption tests. These fixtures
do not exhaust every possible scheduler/firmware interleaving. Three existing VM
regressions also passed: parked refusal/save/reset, Text/file/cache coexistence
and notification-return retirement.

The physical Air's accepted result is still parked query10 **refusal**, release
`a72409383258a4a0`. No SMBus event handling is physically accepted. The v46 implementation below
provides the reviewed ownership/capture boundary; exact-stage validation and
physical acceptance remain separate from the original VM study. Only after a dedicated native result should mounted Home or
ordinary startup be considered. Other models need their own captured profiles.

## Parked native capture (v46, physical acceptance pending)

The explicit `--battery-smbus --laptop-probe-volume <this-unit-uuid>` recipe uses
`srvros.capture=battery-smbus`. It requires automatic return, pauses logging and
unmounts the outer workspace before claiming the controller. The separate VM
study command still refuses in the native kernel. Ordinary query dispatch still
refuses query10; only this capture can use the new profile capability.

The native capability is supplied only after the existing exact Air DSDT/eight
SSDT, PCI, NVS, EC and event-resource checks. No fingerprints, register gates or
sibling identities were widened. The runtime also checks the host `_HID`, `_EC`,
SBS interface and absent/zero EC and SMBus `_GLK`. The captured table has no
`_GLK` request. AML SMBus helpers share SMTX, run only on the runtime worker, and
cannot interleave a sampler or event operation. The capture adopts the host
interface directly in either OSYS state; it neither uses AML `_BST`/BSSW nor
pretends that SMB0's OS-dependent `_STA` remains constant. No firmware method is
patched, and no battery notification or EC query is injected on the laptop.

This is an explicit ownership choice for the bounded diagnostic, not a general
ACPI device-enumeration rule. Known alarms invalidate the direct SBS cache; the
payload is recorded without changing firmware-private battery state. Unknown
senders remain distinguishable. Other rejected queries still take the existing
contained fault reset path, with no claim that unknown AML can safely execute.

The capture waits at most 30 seconds for at least one natural controller event
and a fresh sample at generation 3 or later. It then stops, masks sources, drains
the worker, checks idle transport/interpreter resources, disconnects and restores
the existing event enables. It allows at most 128 recorded events. The final
receipt correlates event/alarm/ack counts, cache invalidation, a fresh pre-stop
observation and nonfresh stopped cache. A late alarm invalidating that observation
makes the run incomplete rather than silently accepting stale data.

After clean retirement, the ordinary capture path remounts, saves, flushes,
unmounts and reboots to Ubuntu. No-event/profile refusal is saved as incomplete.
A poisoned protocol or unsafe teardown resets while storage is parked, without
attempting filesystem writes from the fault context. Such a run can have only
its start logs; missing completion is failure/incomplete, never a pass. No human
interaction is required, and the capture does not intentionally halt for a photo.

`tools/battery_native_smbus_smoke.py` exercises the actual boot/capture/save/reset
path on disposable Q35 storage. `battery_native_smbus_records.py` is shared with
the Ubuntu collector; it rejects wrong native/VM resource identities, missing,
duplicate, torn or inconsistent records and false freshness/retirement claims.
Use exact-stage `hardware_boot_smoke.py --probe-check`, including its missing
workspace case, before installation. Keep the accepted desktop recovery and
prior parked-refusal payload independently backed up. Collect the matching
session, verify prior files and offline exFAT allocation before another run.
