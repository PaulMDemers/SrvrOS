# Native EC and battery runtime gate

Updated 2026-10-19. This is the implementation boundary for the next opt-in
MacBookAir7,2 battery/AC diagnostic. Live battery readings are **not enabled**.
The existing desktop, isolated AML probe, brightness profile and Q35 adapter
retain their current hardware permissions. Other Macs need independent evidence.

## Resource contract implemented

`acpi_ec_resources_parse` decodes ECDT and the relevant FADT SystemIO fields
without accessing hardware. It validates exact lengths/checksums, a bounded
absolute EC path, byte-wide EC ports, legacy/extended address agreement, PM1A
control, SMI mode-switch commands, SCI and the selected GPE0/GPE1 byte/mask.
Conflicting or overlapping decoded ranges, unsupported widths, PM1B control,
hardware-reduced layouts and out-of-range GPEs are refused. A failed parse clears
the output. This is a deliberately supported subset, not a general ACPI parser.

For GPE blocks, zero GAS bit width is accepted only with a valid independent
FADT byte count. A 32-byte block cannot encode 256 bits in that one-byte field.
An extended address of zero falls back to the legacy field; populated legacy and
extended addresses must agree. Legacy 116-byte and at-least-244-byte FADTs are
supported. A missing SMI command is representable, but cannot authorize a mode
transition. PM1 event, PM2, timer, reset, PCI and AML memory permissions remain
outside this decoder; its overlap check covers only the ranges it decodes.

`acpi_ec_namespace_matches` compares integer `_UID`/`_GPE` and a bounded `_CRS`
containing exactly two fixed one-byte IO resources, data first and command
second, then EndTag. Optional resource checksum, decode width, alignment, range
and length are checked. Other resource forms are refused, not silently ignored.

The offline namespace tool adds:

```sh
tools/host-env.sh make test-acpi-ec-resources test-acpi-namespace
build/tools/acpi-namespace --darwin \
  --ec-contract /path/to/ECDT.dat /path/to/FACP.dat \
  --region ADDRESS:/path/to/GNVS.bin \
  --region ADDRESS:/path/to/PNVS.bin \
  --pci SEG:BUS:DEV.FN:/path/to/pci.bin \
  /path/to/DSDT.dat /path/to/SSDT*.dat -- '\PINI' '\OSDW'
```

Use addresses from that unit's captured firmware, never this Air's values as
another machine's configuration. Contract tables are decoded separately; they
are not installed into the interpreter's synthetic table hierarchy. After the
explicit evaluations, the tool resolves the ECDT path to a Device, checks integer
or string `PNP0C09`, reads `_UID`, `_GPE`, `_CRS` and optional integer `_GLK`, and
prints `MATCH` or `REFUSED`. Errors, denied accesses and leaked interpreter
allocations fail the process. `_GLK=1` is reported as requiring a Global Lock;
a resource match does not satisfy that requirement. An absent `_GLK` is reported
as absent, never as proof of exclusive firmware ownership. No live EC handler,
namespace initialization, ACPI mode switch or interrupt is enabled.

## Captured Air7,2 evidence

The October 19 offline replay used the captured DSDT plus eight SSDTs and private
GNVS, PNVS and PCI snapshots. DSDT SHA-256:
`64d28c86c389dbec90d322e17bb0fa2111246c3a41910529c52ca041eb39ad31`.
Raw firmware stays in ignored local evidence. The contract matched with zero
errors, denied accesses or remaining interpreter allocations; four writes changed
private memory copies only. This is firmware-description evidence, not a live
controller test.

| Field | Captured result |
| --- | --- |
| EC path / HID / UID | `\_SB.PCI0.LPCB.EC`, `PNP0C09`, integer 0 |
| Data / command ports | `0x62` / `0x66` |
| SCI / EC GPE | 9 / `0x4e` |
| Selected status / enable / mask | `0x1889` / `0x1899` / `0x40` |
| PM1 control / SMI command | `0x1804` / `0xb2` |
| ACPI enter / leave commands | `0xf0` / `0xf1` |
| `_GLK` | Absent from the loaded EC device |
| Darwin `OSDW`, SMB0 `_STA`, `_EC`, SBS0 `_SBS` | 1, `0xf`, `0x2010`, 1 |

A separate private replay of `PINI`, PCI0 `_INI` and `OSDW` deliberately stopped
at a denied SystemIO mapping: `\GPIO`, base `0x800`, length `0x400`, while
storing `GI82`. It retained Darwin policy but failed with one denied mapping and
zero remaining allocations. This is a useful refusal result, not an initialized
namespace. Audit the exact GPIO field access and its effects before any native
mapping; allowing the whole 1 KiB region would be an unjustified shortcut.

Three initialization details matter:

- PCI0 `_INI` independently tests Darwin, Linux and Windows vendor interfaces.
  Advertising Darwin alongside default Windows responses can overwrite the
  selected OS policy. Disable other vendor interfaces before loading AML and
  explicitly select Darwin for this audited profile. The isolated `PINI` method
  uses a different conditional chain; its success alone does not test `_INI`.
- EC `_REG` connection writes ECSS at offset `0x10`, updates lid/display/AC state
  and notifies the AC device. Full initialization is not read-only even when
  the eventual public API only returns battery readings. Memory/PCI/EC writes
  require an explicit audited permission plan; snapshots cannot substitute for
  the live state when methods control hardware.
- EC queries include SMBus/battery work and AC/lid/display updates. The controller
  owner must dispatch `_Qxx` after releasing the byte/bank transaction lease.
  Dropping queries or running them inside a battery transaction is insufficient.

## GPIO initialization replay and per-access constraint

The next offline replay supplied a one-byte private model at `0xb90`, initially
`0xa5`, and evaluated `PINI`, PCI0 `_INI` and `OSDW` under Darwin-only vendor
interfaces. PCI0 `_INI` completed with exactly one 8-bit read followed by one
8-bit write, `0xa5` → `0xad`. The eight private memory writes, EC contract match,
zero errors/denials/leaks and unchanged model file are recorded locally.
This removes the earlier *offline* mapping blocker; it does not accept a live
GPIO write or full namespace initialization.

The captured field is `GI82`, bit 3 at `GPIO + 0x390`. The upstream Intel-authored
[Linux Lynxpoint pinctrl driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/pinctrl/intel/pinctrl-lynxpoint.c)
describes per-pin configuration at `0x100 + 8 * pin` and bit 3 as interrupt
trigger inversion. That agrees with pin 82's offset. This identifies an interrupt
polarity effect, not a harmless scratch variable; the Mac's wiring, current pin
mode, event state and native ownership still require validation. No Linux driver
code was copied into SrvrOS.

The offline tool supports:

```sh
# Any modeled writes remain in process memory; the input file is unchanged.
build/tools/acpi-namespace --io-model ADDRESS:FILE ...
# One-byte model: only SET_MASK bits may change, and only from zero to one.
build/tools/acpi-namespace --io-set-model ADDRESS:SET_MASK:FILE ...
```

For the captured Air initialization replay, `--io-set-model 0xb90:8:FILE`
admits the observed GPIO access. The declared 1 KiB operation region may be
mapped as an opaque handle, but each access must fit inside the explicit byte
model. Adjacent bytes and wider accesses remain denied. A 4,096-access budget
bounds trace size; exhausting it fails the offline process. No host port or
memory device is opened. Unmodeled SystemIO remains denied by default.

`acpi_io_rule` is the reusable pure predicate behind the restricted model. It
checks exact port and access width, valid masks, unchanged protected bits and
set-only bits. Tests enumerate all 65,536 before/after byte combinations and
cover invalid rules, read-only rules, 16/32-bit values and address overflow.
This predicate is a constraint on an access already admitted by a native owner,
not evidence of ownership or a hardware binding. A future adapter must obtain a
fresh before value under its serialization policy, validate the exact firmware
and live register description, and only use this rule for audited ordinary R/W
registers. It must not apply read/modify/write rules to W1C, command/status or
read-side-effect registers. Native startup links no caller of this predicate.

The synthetic AML suite checks preservation, a denied clear, denied other-bit
changes, outside/wide accesses, private files, malformed models, access budgets
and a Darwin policy that excludes Linux/Windows vendor responses. The captured
Air also succeeds with the set-only bit-3 rule and refuses a wrong bit-2 rule.

## Memory/PCI trace and remaining system initialization

The offline probe now traces each private SystemMemory and PCI access with its
firmware address/device, width, prior value and resulting/read value. The two
snapshot types share a 4,096-access limit; exhaustion fails the process. This is
separate from the SystemIO limit. Synthetic tests cover byte/64-bit memory,
private PCI writes, unchanged input files and runaway memory/PCI methods. No
host physical memory, PCI configuration or port device is opened.

Replaying the same captured nine tables with `PINI`, PCI0 `_INI` and `OSDW`
produced 53 byte memory reads, eight byte memory writes, one 32-bit PCI read and
the previously audited byte GPIO read/write. All eight memory writes were the
low/high bytes of `OSYS` at GNVS offsets 0 and 1: each method first selects
`0x07dc`, then Darwin `0x2710`. The PCI read was device `0:0:1f.3`, offset `0x20`.
PNVS was read but not written on this path. These counts include table loading
and the explicit evaluations, not an exhaustive list of future runtime accesses.
The EC contract matched with zero errors/denials/remaining allocations, and all
captured input files were unchanged.

A separate `_SB._INI` replay with the same permissions fails at an 8-bit read
of GPIO offset `0x274` (port `0xa74`). Captured AML sets field `GN46`, bit 2 of
that byte, after `PINI` and `DSPI` on the Darwin path. Its preserve write first
needs that read; no write was admitted. Four private OSYS byte writes precede
the refusal, so a failed initialization is not necessarily side-effect-free.
The GPIO field's live meaning, ownership and lifecycle still require an audit;
the probe does not admit this byte merely because it shares the GPIO region.

These traces narrow the native permission plan but do not authorize it. A
snapshot cannot establish live NVS ownership or substitute for the FACS Global
Lock. In particular, captured PNVS fields request `Lock`, regardless of the
absence of EC `_GLK`. Full namespace initialization, EC `_REG`, query methods
and teardown must be audited separately from the explicit methods above.

## Complete offline initialization audit

`--initialize` additionally runs `uacpi_namespace_initialize` after table loading
and before explicit query paths. The default remains load-and-evaluate only.
Initialization is refused on an API failure, any logged AML error or warning,
or any denied access, including faults left by table loading. A prior load fault
skips initialization entirely. A refused initialization skips both follow-up
queries and the EC contract check, then releases the interpreter and reports
remaining allocations. Warning refusal is intentionally strict for this audit;
it does not imply every warning is a hardware fault.

```sh
# Add to the same explicit, unit-specific snapshot/model arguments above:
build/tools/acpi-namespace --initialize --darwin ... -- '\OSDW'
```

This is a reduced-hardware **offline** interpreter. It does not install an EC
handler, run EC `_REG` as a connected device, exercise FACS firmware arbitration,
or activate SCI/GPE hardware. Even an offline PASS would not accept those paths.
The existing wall-clock, AML loop, allocation and per-access limits remain in
force. Synthetic cases verify opt-in initialization effects, ignored `_INI`
failure, malformed `_STA`, prior load failures, skipped queries and cleanup.

The Air's `GN46` field corresponds to pin 46's configuration register 2, bit 2:
`0x104 + 8 * 46 = 0x274`. The Intel-authored
[Linux Lynxpoint driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/pinctrl/intel/pinctrl-lynxpoint.c)
identifies that bit as disabling input sensing. This is a register-layout
inference, not evidence of the Mac's board wiring or permission to alter it.
Captured AML also changes this field in sleep/wake paths. Those methods were
not executed, and no Linux implementation was copied.

A private one-byte set-only model at `0xa74`, mask `4`, lets the explicit
`_SB._INI` replay complete: one byte read/write, `0xa1` to `0xa5`, preserving
all other bits; four private OSYS byte writes; no errors/denials/leaks. The
model file is unchanged. Supplying both this model and the earlier GI82 model
then exposes the following dependencies during complete namespace initialization:

| Captured dependency | Result / remaining requirement |
| --- | --- |
| HPET `_STA` reads `HPAE` in `RCRB` | Mapping at `0xfed1c000`, length `0x4000`, refused. AML declares a DWord field at offset `0x3404`, bit 7; audit an exact read permission instead of allowing the whole chipset region. |
| PCI `0:0:3.0` and `0:0:1f.0` | Missing snapshots refused; offsets/directions are not established by a failed device-open. Need evidence before permitting accesses. |
| OSYS and GPIO | Eight private OSYS byte writes and both constrained GPIO read/writes occurred before the final refusal. Initialization is not transactional. |

The captured full replay returns API status zero but records four AML errors and
11 denied accesses. The tool reports `REFUSED`, skips `OSDW`/contract evaluation
and releases all interpreter allocations. There are 101 private memory reads,
one PCI read, and no input-file changes. This is a useful failed audit, not a
complete native permission list: missing models and the absent EC handler can
prevent later paths from running.

The pinned full uACPI Global Lock implementation in `source/mutex.c` also needs
specific containment before native use: it waits for a firmware release event
with timeout `0xffff` inside acquisition, and release can write PM1 `GBL_RLS`.
The existing Q35 policy denies PM1 control writes; it must not simply be widened
for the native case. A bounded ownership/watchdog path, exact release-bit
semantics and contended firmware handoff tests remain required. The offline
reduced-hardware build does not exercise this handshake.

## SCI route ownership implementation

`runtime/sci_owner` separates the route/callback lifetime from the Q35 adapter.
It resolves the supported ISA SCI description, including an explicit remapped
GSI, and treats conforming SCI flags as active-low/level. Invalid/edge flags,
reserved IRQ stubs and mismatched override sources are refused. The IO-APIC
backend must separately validate the GSI's actual topology and write readback.

Preparation requires a masked route with no delivery/remote-IRR pending state.
It saves the route, prepares it while masked, then registers the callback.
Enablement happens separately, so an interrupt delivered immediately on unmask
has a valid callback and context. A failed partial prepare or registration rolls
back through a verified mask and route restoration. A failed unmask is masked
again before it can be retired. Removal masks first, removes the callback, then
restores the saved masked route. Mask/remove/restore failure poisons the owner:
its state is retained, subsequent reuse is refused, and the adapter must halt.

This owns the interrupt route and callback only. The lifecycle caller still
owns firmware matching, source configuration/quiescence, CPU0 serialization and
worker draining before freeing context. It does not provide Global Lock, EC
transactions, native ACPI mode or GPE dispatch. The separate Q35 adapter uses
this common owner while retaining its strict Q35/SCI-9/GSI-9 gate; accepting a
remapped GSI in the host fixture is not physical machine acceptance.

## Bounded EC query draining

`runtime/ec_query_budget` accounts for one notified drain from initial admission
through every worker batch. The separate Q35 EC fixture uses a maximum of 128
query attempts and two seconds per drain, with eight attempts per worker turn.
Another interrupt or a rescheduled batch does not reset either budget. Queued
time behind a complete SBS sample counts too. Limits are diagnostic policy for
this fixture, not accepted latency/throughput limits for the native Air EC.

The worker releases the EC transaction lease before invoking `_Qxx`. It checks
the budget before a transaction, after the transaction, after AML, and before
rescheduling or declaring completion. A zero reply with the model's event still
asserted retries within the same budget. A quiet source cannot erase a missed
deadline. Clock regression, exhausted attempts, expired time, transport/AML
failure or scheduling failure retain the first fault and reject reuse.

This is a cooperative deadline: it detects time spent inside AML when that call
returns; it does not interrupt or cancel an executing method. The lifecycle's
independent worker-drain deadline is still required for a method that does not
return. On query failure the disposable VM fixture stops admitting EC work and
halts visibly with interrupts disabled and its worker/namespace retained. It
does not fabricate a clean disconnect or reset the transport.

There is no native GPE binding in this fixture. A native owner must suspend the
selected source while draining, acknowledge according to its trigger semantics,
and close the pending-event/rearm race under serialization. Budget success
alone cannot authorize those register accesses or claim a quiescent controller.
The fixture's source-pending check and drain completion are serialized on CPU0;
its synthetic event flag must not be substituted for hardware GPE/EC status.

## Next adapter and acceptance sequence

### Event-register ownership audit

The pinned full uACPI runtime touches more than the selected EC event. In
`source/event.c`, `initialize_fixed_events` disables each supported fixed-event
enable field. `create_gpe_block` writes zero to every GPE enable byte and `0xff`
to every GPE status byte before SCI installation. `uacpi_initialize_events`
also installs/enables the fixed Global Lock handler. `NO_ACPI_MODE` does not
suppress this initialization. Some fixed-event initialization write results are
discarded by upstream, so adapter denial/error counters must gate readiness as
well as API status. No upstream code has been modified.

`acpi_event_resources_parse` now validates the complete supported FADT SystemIO
layout separately from the earlier EC-address contract: 16-bit PM1 status and
enable halves, 16-bit PM1 control, optional 8-bit PM2 control, optional 32-bit
timer, SMI command and both GPE blocks. It checks bounded tables/checksums,
legacy/extended agreement, address ranges, widths and overlapping domains.
PM1B and reduced-hardware layouts remain unsupported. The captured Air decodes
to PM1 status/enable `0x1800/0x1802`, control `0x1804`, PM2 `0x1850`, timer
`0x1808`, and 16 GPE status/enable bytes at `0x1880/0x1890`, with no GPE1.
This is offline layout evidence only. The native adapter must additionally
cross-check these domains against EC ports and its other live mappings.

`runtime/event_io` is a pure access policy built from this validated layout.
Mappings/accesses must match one register at its exact width; a broad range
does not admit narrower, wider or neighboring accesses. Status writes are
treated as write-one-to-clear, without an ordinary-memory read/modify/write
predicate. Enable writes may set only explicitly allowed bits. PM1 control,
PM2 control and timer writes are denied, as are SMI reads and commands other
than the decoded enter/leave values. Reset/sleep registers and unrelated IO
are not mapped.

The Q35 adapter uses this policy with only fixed Global Lock and power-button
enable bits, no GPE enable bits, and exclusive ownership of its disposable,
initially disabled event domain. Ordinary native startup has no binding. A
future native owner must establish ownership of the **whole status domain**
before allowing uACPI's initialization clears, even if it later enables only
the EC GPE. Masking just that GPE is insufficient. Pending status cleared during
initialization cannot be restored from a snapshot; it must not be advertised as
reversible rollback. FACS Global Lock access and AML NVS/PCI/GPIO permissions
remain separate unresolved gates.

### Global Lock lifecycle fixture

The separate Q35 runtime now exercises the pinned full uACPI Global Lock API.
`acpiruntime global-lock` queues acquisition/release on the ordinary worker and
checks FACS Owned/Pending state, API status and adapter error/denial counters.
Two complete runtime lifetimes return with zero heap/errors/denials. The
fixture validates its FACS signature/length and an initially free lock, then
admits only the exact 32-bit lock-word read while that explicit fixture is
active. Other MMIO reads/writes remain denied. uACPI's lock updates use atomic
compare/exchange directly; they do not pass through the MMIO write callback.
This is another reason a native FACS mapping requires its own ownership policy.

Two disposable VM faults cover the boundaries:

- `global-lock-block` models firmware holding Owned before acquisition. The
  worker sets Pending and blocks on the real uACPI firmware-release semaphore.
  Shutdown masks event sources/SCI, then times out draining the worker **before**
  calling interpreter reset. The halt retains the same nonzero heap, lock bits
  `3` and registered masked SCI owner. Interrupts are verified disabled. This
  does not simulate a successful firmware notification or accept native SCI.
- `global-lock-release-denied` sets Pending after acquisition, making release
  attempt PM1 `GBL_RLS`. The existing control-write policy rejects it. Upstream
  release still returns success and clears the lock bits; adapter accounting
  catches the denial and halts with runtime state retained. No safe rollback or
  completed firmware handoff is claimed, and the policy is not widened.

The lifecycle owner now drains work before `uacpi_state_reset`, because upstream
retires event machinery before invoking its own work-drain callback. The latter
remains a defensive check. A timeout does not close the firmware semaphore,
remove the SCI callback, free live interpreter state or fabricate an unlock.
This is bounded shutdown containment, not yet an autonomous watchdog for a
worker stuck during ordinary operation.

Run sequentially after `make acpi-runtime-vm-kernel`:

```sh
python3 tools/acpi_global_lock_smoke.py
python3 tools/acpi_global_lock_smoke.py --fault block
python3 tools/acpi_global_lock_smoke.py --fault release-denied
```

CI retains all three cases plus the existing SCI and EC regressions. Normal
startup/native adapters are unchanged. Successful contended firmware handoff,
exact native PM1 release permission, validated FACS/NVS mappings and an
independent operation deadline remain prerequisites for a native battery trial.

### Integration sequence

1. Preserve separate restricted-probe and full-runtime build profiles. Match the
   actual firmware/profile and validate resources before granting access. Audit
   every live table/NVS/FACS/register mapping needed by the selected namespace;
   do not turn the offline snapshot allowlist into unrestricted physical access.
2. Establish the full initialization owner, early EC handler and Global Lock
   policy before module-level AML. Reconcile the namespace device after load;
   refuse changed identity/resources or interpreter errors even when a uACPI
   API returns success. The merged Q35 early-load fixture tests the worker
   mechanism, not this machine's ownership policy.
3. Resolve SCI through this machine's MADT/IO-APIC topology. Prepare and verify
   the masked level-triggered route, retain previous state, install callback and
   worker lifetimes, then perform the explicit ACPI-mode transition and verify
   SCI_EN within a deadline. Full uACPI initialization touches event registers
   even with `NO_ACPI_MODE`; account for those writes before calling it.
4. Run approved initialization and `_REG` under the EC owner, with error/denial
   accounting. Register/query the EC GPE and bound event draining before enabling
   event users. Do not enable every firmware GPE merely to obtain battery data.
   General namespace/GPE finalization needs its own permission audit.
5. Serialize whole SMBus bank transactions, AML accesses and queries on the
   ordinary worker. Bind `sbs_sample` only after successful ownership. Publish
   complete, timestamped, unit-aware cached snapshots; UI polling must never
   perform EC I/O. Distinguish absent AC evidence, stale battery data and a
   poisoned transport from valid zero values.
6. On failure or shutdown, reject new work, mask owned sources, synchronize
   interrupts and drain workers. Disconnect EC handlers while the namespace and
   owner still exist. If a worker or controller cannot be quiesced, retain its
   storage and halt visibly with durable diagnostics. Do not free live state or
   attempt a blind EC reset. Restore prior routing/mode only where ownership and
   rollback have been established and tested.
7. Before a native trial, exercise the concrete adapter's initialization, denied
   mapping, `_REG` error, GPE storm and teardown failures in host/VM fixtures.
   Stage an opt-in exact release with log flush and automatic return on success,
   visible halt on failure, Ubuntu recovery and independent accepted payload.
   First collect a bounded AC-connected sample. Physical AC unplug/replug,
   sensible fresh readings and responsive input/storage remain later explicit
   acceptance; VM or Linux values cannot accept those workflows.

CPU control, lid policy, sleep and suspend/resume are separate milestones.
See the [uACPI port record](../ports/srvros/uacpi/PORT.md),
[laptop service plan](macbook-air-laptop-services-2026-10-18.md#live-acpi-runtime-integration-plan)
and [machine ledger](machines/macbookair7-2.md).
