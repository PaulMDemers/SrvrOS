# Native battery AML side effects

Source review based on `d49ef02` (PR #40), captured **MacBookAir7,2** firmware and
the pinned in-tree uACPI implementation. This completes the static mapping needed
to choose the next recovery experiment. It does not enable native mounted Home,
automatic battery startup, new register access or another model's firmware.
Read with the [resource audit](battery-resource-audit.md) and
[service contract](battery-service.md).

## Decision

Do not treat an idle EC plus a free Global Lock as proof that arbitrary AML can
be abandoned. Startup changes shared firmware state, `_REG(3,0)` does not undo
those changes, and the query dispatcher can reach methods outside battery status.
The next implementation should record operation boundaries and side effects,
then propagate a **selected failure that returns normally** to the service owner.
It must first pass disposable VM tests; native admission remains a separate step.

Keep namespace-load, connection, initialization, disconnect, partial EC/SMBus,
firmware-lock waits and arbitrary interpreter faults terminal for now. Do not
force-clear a protocol, restore stale NVS bytes or release a retained thread's
lock to manufacture a recoverable state.

## Evidence and reproduction

The local audit checked DSDT plus all eight SSDTs against the prior capture's
SHA-256 manifest, declared lengths, ACPI checksums and the exact fingerprints in
[battery_native_platform.c](../ports/srvros/uacpi/runtime/battery_native_platform.c).
The DSDT is 35,034 bytes, fingerprint `e0f810cb41f75c0b`. The eight SSDTs are:

| Captured name | Table ID | Bytes | Native fingerprint |
| --- | --- | ---: | --- |
| SSDT1 | SataAhci | 267 | `7b1010f6c77d5eb9` |
| SSDT2 | SmcDppt | 36 | `3855ae741a19efe5` |
| SSDT3 | SsdtS3 | 50 | `9a34aad14986eaf5` |
| SSDT4 | PcieTbt | 11,046 | `f7634c379cd72fe5` |
| SSDT5 | Sdxc | 184 | `c010202e36fbc93d` |
| SSDT6 | Xhci | 2,732 | `ec2802855c9ba1f7` |
| SSDT7 | Cpu0Ist | 1,527 | `196a92da329719e3` |
| SSDT8 | CpuSsdt | 3,095 | `2d3515d0b8520937` |

Raw firmware stays in ignored local evidence, with a hash-verified copy outside
`build`. Do not publish full AML or assume another unit has matching tables.
For a local review, copy the nine verified `.dat` files into a fresh audit
directory. Run one `iasl -e <all other tables> -d <target>` process per table,
through `tools/host-env.sh`, keeping stdout/stderr and tool version. This avoids
namespace collisions encountered with this tool's multi-target `-da` invocation.
Review the resulting methods and their field definitions against the actual
adapter gates; text searches alone are not an AML reachability proof.

The disassembler is iASL 20251212. Even with all eight SSDTs as externals it reports
two unresolved DSDT methods, `HDOS` and `HNOT`; both references are guarded by
`CondRefOf`. They occur in other display methods, outside the entry paths below.
This is a bounded source audit, not an assertion that every possible firmware
method, dynamic table or SMM action is known. SSDT8 contains dynamic `Load` paths
under processor `_PDC`/`_OSC` helpers; the battery runtime does not invoke those
processor interfaces. Adding such callers invalidates this scope.

## Actual entry points and order

[battery_runtime.c](../ports/srvros/uacpi/runtime/battery_runtime.c) installs the
EC region handler at the namespace root **before** `uacpi_namespace_load()`.
Loading executes table bodies, not just method declarations. Region address
expressions can read shared fields, including the Lock-rule PNVS fields in SSDT8;
therefore namespace loading itself belongs inside the ownership audit. The
reviewed table bodies principally declare names, buffers, regions, fields and
methods; that observation does not make region-expression evaluation inert.

The runtime then checks the EC identity/resources, installs a notification
handler, explicitly connects EC regions, and initializes the namespace. The
pinned [uacpi.c](../ports/upstream/uacpi/source/uacpi.c) initialization sequence
runs root `_INI`, `_SB._INI`, default-handler `_REG` methods, then device `_STA`
and `_INI` traversal. It is not restricted to the battery device. This captured
set has two `_INI` methods (both in DSDT), one EC `_REG`, and nine EC `_Qxx` methods;
none of the eight SSDTs adds these entry methods.

The 19 DSDT `_STA` methods read interrupt-link PCI bytes, HPET enable state,
`OSYS`, Darwin selection or the HDAU vendor ID, or return constants. BAT0's
non-Darwin branch additionally invokes `UBSS` and SMBus helpers; under the
configured Darwin policy it returns absent before that call. SSDT4's 72 `_STA`
methods return constants or select a constant using `OSDW`. These reads explain
why namespace startup requires more than the EC data/command ports.

The port disables vendor interfaces and installs `Darwin`. `PINI` and PCI0
`_INI` select `OSYS=0x2710`; `OSDW()` tests that shared GNVS field, rather than
consulting a private immutable flag. The non-Darwin branches are therefore
excluded by the intended state, **not** by a permanent method-level allowlist.

| Entry / helper | Direct effects under intended Darwin state | Boundary consequence |
| --- | --- | --- |
| EC `_REG(3,1)` | Sets private `ECOK`; writes EC `ECSS=0`; reads lid/AC bits; writes GNVS `LIDS`, `PWRS` and graphics `CLID`; queues ADP1 notification | Can fail after a strict prefix of shared writes; no rollback transaction |
| `_SB._INI` → `PINI`, `DSPI` | Writes GNVS `OSYS`; Darwin makes `DSPI` skip its chipset mailbox sequence; sets GPIO `GN46` | Partial initialization is not a completed lifetime |
| PCI0 `_INI` | Writes `OSYS`, sets GPIO `GI82`; Darwin skips Thunderbolt/NHI/GPIO reconfiguration | Do not widen PCI/MMIO gates to make another branch pass |
| EC `_REG(3,0)` | Sets private `ECOK=0`; the connection-only body is skipped | Does not restore ECSS, GNVS, CLID or GPIO state |
| Notify delivery | Adapter increments counters on the worker; periodic sampling supplies readings | It does not invoke a device driver, nest EC I/O, change CPU policy or implement lid sleep |

Default-handler `_REG` calls do not imply all field accesses are private. The
pinned [opregion.c](../ports/upstream/uacpi/source/opregion.c) can execute `_REG`
before detaching a handler, while that handler is still usable. Successful
attach/detach accounting only proves the adapter's region lifecycle.

## Shared fields and ownership

These names were resolved from the captured field layouts and compared with
[battery_native_policy.h](../ports/srvros/uacpi/runtime/battery_native_policy.h).
They describe existing permission, not newly granted access.

| Field / resource | Existing access | Ownership / recovery limit |
| --- | --- | --- |
| GNVS `OSYS`, offsets 0–1 | Byte writes implementing a 16-bit OS identity | Shared firmware state; a partially completed store can contain a mixed value |
| GNVS `LIDS`, `PWRS`, offsets 15,16 | Byte writes restricted to 0/1 | Shared lid/AC snapshots; no acknowledgement or transaction completion token |
| IGDM `CLID`, offsets `0x1ac..0x1af` | Low byte 0/1, remaining bytes zero | Four byte stores implement a 32-bit graphics mailbox field; current backend does not ring a graphics doorbell or own the display firmware |
| GPIO `GN46` | I/O `0xa74`, only mask `0x04` may be set | Shared chipset bit, read/modify/write preserving others; no lease or inverse operation established |
| GPIO `GI82` | I/O `0xb90`, only mask `0x08` may be set | Same limitation; normal runtime close does not restore it |
| EC `ECSS` at `0x10` | `_REG` writes zero through the worker transport | AML assignment is known; private EC firmware meaning/rollback is not established |
| EC `ELSW`/`EACP` at `0x60` | Serialized reads for lid/AC | One completed byte read is narrower than a complete multi-field method |
| EC SMBus bank `0x20..` | Sampler sets address/command/protocol and polls through completion | Has external protocol ownership beyond a single EC byte; timeout poisons it |
| PNVS | Readable captured range, no permitted normal writes; fields use `Lock` | Even reads may use the firmware Global Lock; ordinary MMIO write denial does not block its FACS atomics |
| FACS Global Lock / PM1 GBL_RLS | Pinned interpreter handshake and narrowly permitted release notification | Preserve API/AML/semaphore/pending/FACS distinctions from the resource audit |
| ACPI mode / fixed events / EC GPE / foreign GPEs | Dedicated event policy, SCI owner and saved foreign enable state | Separate lifecycle from AML fields; masking delivery does not stop SMM |

Only the normal mode transition's SMI command is admitted by the event policy;
normal close restores the original mode when needed. That is an explicit firmware
interaction, not proof that the firmware has no other side effects. DSDT also
declares SMI/trap/mailbox helpers (`TRAP`, `SPRT`, chipset I/O regions). The traced
Darwin entry paths do not call `TRAP`; arbitrary SMI ports, GNVS `SMIF`, chipset
mailbox writes and PCI writes remain denied. The byte-write allowlist does not
make permitted multi-byte stores atomic, and no stale snapshot restoration should
be added without a shared-resource owner.

## All captured EC query methods

Every nonzero QR_EC response currently constructs `_Qxx` and calls
`uacpi_execute_simple`. Missing methods and errors are terminal. No method-class
admission or side-effect ledger exists yet. QR_EC itself consumes a controller
query; declining its AML method cannot be described as replaying or undoing it.

| Code | Intended Darwin behavior | Other effects / exclusion |
| --- | --- | --- |
| `10` | Notify SMB0 of status change | Non-Darwin path acquires SMTX, examines SMBus alert fields, may call BAT0.BNOT and writes SSTS=0. Do not classify it solely from its code without the OS-state contract |
| `20` | Reads ELSW, writes LIDS and CLID, notifies LID0 | Partial shared multi-store update possible; not merely a notification |
| `21` | Reads EACP, writes PWRS, notifies ADP1, calls PNOT | PNOT reads TCNT/private PDC values and sends CPU notifications; it does not itself write CPU controls |
| `40` | Notify ALS0 | Adapter currently counts only |
| `5a` | Notify sleep button | Adapter currently counts only; no sleep implementation |
| `80` | Notify CPU0–7, then inspect IGPU vendor ID; Intel branch reads IGPS/RP0C and writes RPSL | Reaches GPU/chipset power policy: IGPS is I/O `0x311`, RP0C is MMIO `0xfed15998`, RPSL is MMIO `0xfed15994`. These are not permitted; IGPU PCI opening is also refused. Earlier notifications may already have queued |
| `ce` | Empty body | QR_EC has still occurred |
| `cf` | No action under Darwin | Non-Darwin path notifies sleep button |
| `d0` | Empty body | QR_EC has still occurred |

The denial of query `80` is intentional containment under today's gates. This
review did not reproduce that query on hardware and does not claim it caused a
previous failure. Supporting it needs an explicit GPU/power ownership decision,
not a wider address allowlist. Notification delivery is asynchronous; normal
method return does not by itself prove the work queue is empty.

## Normal-return boundary selected for the next implementation

The first candidate is a **ready service's controlled query-admission failure**
after QR_EC completed and before unapproved AML starts. Start in the VM backend.
The owner should observe a structured failure after the worker returns, stop new
work, drain existing callbacks, and use established orderly retirement only if
all invariants permit it. This does not require abandoning an interpreter stack.
The consumed event and any loss of notification must remain visible as a service
failure; never report a successful query or keep presenting a fresh battery value.

Before enabling that path:

1. Record operation kind/generation, query code, AML entry/normal return and
   whether shared writes or unsupported access occurred. Distinguish “not begun,”
   “completed,” and “uncertain”; a zero resource snapshot cannot replace this.
2. Establish an exact native method/OS-state policy. Unknown methods and `80`
   must be refused before AML execution, with an explicit failed-service outcome.
   Do not silently ignore them or rewrite their code to a benign query.
3. Require a healthy completed EC exchange, no lease/SMBus/sampler activity,
   no pending query/callback, no service/storage-owner confusion and all existing
   Global Lock/resource checks before orderly retirement. Drain notifications
   through the worker, outside IRQ context. Never turn a drain timeout into success.
4. Prove ordinary close can return: region disconnect, interpreter/event teardown,
   worker stop, original ACPI mode and foreign-event restoration. If any step
   fails or ownership becomes uncertain, existing containment still applies.
   “Clean retirement” describes that lifecycle; it is not restoration of every
   shared byte written during startup.
5. VM tests must include a normal query, unknown/high-effect predispatch refusal,
   queued notification drain, refusal after partial shared writes, retained lock,
   poisoned/partial EC and disconnect failure. Verify exact failure/save/retire
   records, unrelated storage continuation, automatic reset and offline integrity
   only in cases that reach a valid boundary.
6. After local evidence, use a parked-workspace automated Air capture with the
   accepted recovery retained. Ordinary mounted Home and automatic startup each
   need separate acceptance; static inspection and model faults grant neither.

A successfully returned sample's value-validation failure may be a later candidate
once every SMBus transaction is known complete. An AML error after a permitted
write, setup failure, query deadline or unfinished transaction is not included in
this first boundary. Existing VM stack-retention studies remain valid for their
explicit cases, but are not the default native strategy.

## Known proof gaps

The current interpreter's namespace initialization counts `_STA`/`_INI` errors
without returning those counters as an aggregate failure status. Namespace load
also tracks table-load failures separately. SrvrOS checks return statuses, ERROR
logs and denied accesses, but that is not a general receipt for every method's
success. A future setup recovery path needs explicit completion evidence and
fault tests; returning from `uacpi_namespace_initialize()` is insufficient alone.

The field audit identifies where writes land, not all SMM/EC consumers or their
hardware meaning. There is no firmware lease for GNVS/CLID/GPIO, no rollback for
ECSS, and no guarantee that FACS remains free after a snapshot. Those limits are
why this milestone produces a narrow next experiment instead of enabling native
recovery or ordinary desktop battery startup.
