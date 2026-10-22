# EC SMBus event ownership study

Status: VM-tested study (`1797e8a`); native query10 remains refused. This extends the
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
The native backend cannot grant the VM capability. Its existing predispatch
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
`a72409383258a4a0`. No SMBus event handling is physically accepted. Before a native
trial: review firmware/OS controller ownership, extend the explicit capture and
strict saved proof, validate its exact stage in VM, preserve recovery and park
outer storage. Only after a dedicated native result should mounted Home or
ordinary startup be considered. Other models need their own captured profiles.
