# MacBook Air laptop-service bring-up

Read-only inventory taken from the target Ubuntu host on 2026-10-18, after the
native desktop VM work. No power, display, EC or firmware settings were changed.
The raw local capture is `build/hardware/desktop-power-20261018/`; repeat with:

```sh
python3 tools/capture_linux_hardware.py
```

The collector now includes `laptop-state.json`, an explicit allowlist of Linux
power/backlight properties and firmware/driver associations. It does not read
battery serial numbers or input events. Values retain Linux units and are not
an atomic snapshot. The unprivileged capture records unavailable ACPI-table
reads in `capture.json`; an unread table is not evidence that it is absent.

## Current checkpoint: returned physical device trial

The sections below preserve the bring-up sequence; earlier statements about
unavailable controls describe those earlier stages. Candidate
**fe119d1c99070b18** stalled during optional AML probing. Replacement
**b427b78f21644bd2** (v22) omits that probe and successfully reached the desktop.
The user confirmed brightness and audible volume changes and live sensor output.
PCM file playback, headphone routing and an actual network adapter still need
specific physical acceptance. Implementation/VM results are recorded in the
[device progress](desktop-progress-2026-10-18.md#device-support-block-audio-usb-networking-and-sensors).
The [machine record](machines/macbookair7-2.md) records the exact accepted scope.
Physical secondary clicking is implemented with native acceptance pending, and
System had an input-timeout closure. The [October 19 work](desktop-progress-2026-10-19.md)
records the input fix, timeout investigation and subsequent validation.
Battery/AC, recording, mixing, richer media formats, fan control, lid policy and
suspend remain unfinished. New model agents start with the
[Ubuntu guide](ubuntu-device-agent-setup.md), not the Air's firmware/UUID values.

Next work follows the [shared priorities](apple-hardware-expansion-plan.md#current-work-and-shared-dependencies):

1. Physically accept two-finger secondary click, investigate the System timeout
   and measure idle/polling behavior. The PCM/WAV path is implemented and
   VM-tested, including storage overlap; file playback and headphone routing
   still need specific physical checks.
2. Match an actual USB adapter's captured identifiers/descriptors to its driver
   requirements. The October 18 evening inventory still has no Ethernet adapter;
   CDC ECM VM success does not establish support for an arbitrary purchase.
3. Build a separate live ACPI runtime adapter with scheduler-backed waits/work
   and explicit SCI/GPE/EC ownership. Keep the isolated probe available as a
   diagnostic. Do not turn its snapshot callbacks into live I/O callbacks.
4. Serialize EC transactions and query dispatch through one owner, then bind the
   existing SMBus/SBS engines to a scheduled provider. Publish complete samples
   with age/error state and preserve charge versus energy units. Exercise delayed
   replies, query contention, removal and failed initialization before enabling
   the provider in the normal laptop profile.
5. After the audio route is accepted, add a GUI player using the shared file
   picker and asynchronous worker/job library. Decide the live-volume/mute and
   media-key policy before enabling those shortcuts: the current PCM gain is
   fixed at Open, and changing queued samples is not an instantaneous master
   volume control. Mixing, pause/seek and additional formats are separate work.

Power-off and suspend/resume each need their own storage-flush and device-state
acceptance work. Neither should be inferred from working sensors or battery data.

## Observed target

| Component | Observation |
| --- | --- |
| Machine | MacBookAir7,2; Intel Broadwell-U |
| Display | Intel HD Graphics 6000, PCI `8086:1626`; eDP panel |
| Backlight | `intel_backlight`, type `raw`; observed actual/requested 852, maximum 2777 |
| AC supply | `ADP1`, `ACPI0003`, Linux platform driver `ac`; online at capture |
| Battery | `BAT0`, `ACPI0002`, Linux platform driver `acpi-sbs` |
| Battery namespace | `\_SB_.PCI0.LPCB.EC__.SMB0.SBS0` |
| SMBus namespace | `\_SB_.PCI0.LPCB.EC__.SMB0`, `ACPI0001` |
| Embedded controller | `\_SB_.PCI0.LPCB.EC__`, `PNP0C09` |
| Built-in Wi-Fi | Broadcom BCM4360, PCI `14e4:43a0` |
| USB networking | No USB Ethernet adapter present in this capture |

Battery status and reported capacity are separate observations: Linux reported
`Full`, capacity 86, and zero instantaneous current. The desktop must preserve
provider semantics and distinguish unknown/stale readings rather than invent a
percentage from unrelated fields or infer charging from an AC connection alone.

## Firmware evidence and EC transport foundation

A second, privileged **read-only** capture is retained at
`build/hardware/laptop-services-20261018/`; its capture manifest reports no
unavailable/partial reads. Copies disassembled by the bundled ACPICA `iasl`
are in `build/review/laptop-acpi/`, with the command log at
`build/review/laptop-acpi-disassemble.txt`. The 35,034-byte DSDT identifies
APPLE / MacBookA, OEM revision `00070001`, compiler INTL `20140424`.
Its SHA-256 is
`64d28c86c389dbec90d322e17bb0fa2111246c3a41910529c52ca041eb39ad31`.

The relevant namespace is `\_SB.PCI0.LPCB.EC`:

| Firmware object | Evidence |
| --- | --- |
| EC `_HID` / `_UID` | `PNP0C09` / 0 |
| EC `_CRS` | One-byte data port `0x62`, command/status port `0x66` |
| EC `_GPE` | `0x4e` |
| `ECOR` | EmbeddedControl region, offset 0, length `0xff` |
| EC offset `0x20` | `SPTR`, `SSTS`, `SADR`, `SCMD`, 32-byte `SBFR`, `SCNT`, alarm fields |
| EC offset `0x60` | `ELSW` lid bit 0, `EACP` AC bit 1 |
| `SMB0` | `ACPI0001`, `_EC=0x2010`, serialized AML accesses through `SMTX` |
| `SMB0.SBS0` | `ACPI0002`, `_SBS=1` |

This is resource evidence, not a ready-to-use initialization recipe. Firmware
makes the SMB0 `_STA` result conditional on `OSDW()` (OSYS `0x2710`, selected
through its Darwin `_OSI` path). There is also an alternative `PNP0C0A` battery
path. EC `_REG` writes `ECSS` at offset `0x10` and seeds lid/AC state. `_Q10`
routes battery/SMBus notifications; `_Q20` and `_Q21` update lid and AC state.
No `_GLK` was found in the captured disassembly, but that alone does not prove
exclusive controller ownership or establish correct SCI/query handling.

The new `ec_transport.c` is a callback-based, bounded byte-transaction engine
following [ACPI 6.5, Embedded Controller Interface](https://uefi.org/specs/ACPI/6.5/12_Embedded_Controller_Interface_Specification.html).
Each poll performs at most one status read and one command/data access; it never
spins or sleeps internally. Read/write handshakes retain progress across polls.
Timeout, clock rollback, excess polling, cancellation and unexpected output
poison the transaction state until external recovery explicitly reinitializes it.
Unexpected output bytes are not silently consumed. Host tests model delayed
responses, overlapping-request refusal and those failure conditions.

**There is no hardware binding or caller yet.** This code does not access EC
ports, execute AML, drain SCI queries, read a battery, change brightness or claim
power-management support. Before binding it, resolve namespace initialization,
controller ownership/Global Lock requirements, query/event dispatch and a
scheduled worker. Then build the SMBus/SBS sampler and its cached userspace API.
Firmware bytes and Linux driver implementations were not copied into the driver.

## Battery/AC implementation sequence

1. Capture and inspect this machine's relevant ACPI namespace/resources. Resolve
   the EC I/O resources and SMBus controller configuration from firmware evidence;
   do not assume that finding `_BIF`/`_BST` is the target battery path.
2. Implement the required bounded EC transaction transport, serialization,
   timeout/error handling and query/event ownership. Keep waiting outside GUI and
   interrupt-disabled paths. Decide the AML evaluation strategy explicitly;
   SrvrOS's current ACPI scanner is not a general AML interpreter.
3. Add the EC-backed SMBus host path and read-only SBS sampling. Preserve raw
   state, signed current, unit/scaling metadata, validity and sample age. Start
   with adapter presence, battery presence, capacity and charging state.
4. Publish a versioned cached power snapshot to userspace. System's existing
   properties/table controls can display it; the global status area can consume
   the same snapshot. Unsupported hardware must remain explicitly unavailable.
5. Validate AC connect/disconnect and repeated sampling on the laptop while
   typing and doing file I/O. A stopped or timed-out provider must not freeze the
   desktop or leave a convincing stale “charging” indicator.

This sequence follows the observed ACPI0001/ACPI0002 chain. As protocol references,
Linux's pinned [SBS driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/acpi/sbs.c)
matches ACPI0002 and separates reported state of charge from charging direction;
its [SMBus host-controller driver](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/acpi/sbshc.c)
uses EC transactions and the firmware `_EC` configuration. These files are GPL
references; no implementation was copied into SrvrOS.

## Brightness implementation sequence

The existing `intel_gfx.c` recognizes this Broadwell device but currently maps
only the initial 16 KiB of BAR0 and reports planned acceleration. It does not
provide a panel/backlight backend. Map and validate the required display
register range only after resolving the active panel's control path and firmware
state. Preserve a working panel mode and the existing software framebuffer.

Start with read-only discovery of the current duty/range/control mode. Then add
a bounded brightness provider with clamping, an explicit unavailable state, and
readback; connect it to the shared slider in Settings. Test keyboard and USB
storage responsiveness through repeated brightness changes. Automatic blanking
and lid policy follow only after manual brightness is reliable. Linux's pinned
[Intel backlight implementation](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_backlight.c)
is a reference for the multiple possible panel/backlight paths, not proof that
one register sequence is correct for this machine.

## Acceptance order after this VM-only session

1. Run the expanded native desktop on the laptop: open/edit/save/reopen a USB
   document, use Terminal and Preview, tile/resize, and exercise keyboard and
   trackpad during I/O. Retain the accepted recovery image.
2. Bring up read-only battery/AC reporting, then manual brightness.
3. When a USB network adapter arrives, capture VID/PID, interfaces and chipset,
   then choose the driver target. Preserve the e1000/QEMU regression meanwhile.
4. Treat safe shutdown, lid behavior and suspend/resume as separate acceptance
   gates covering storage flushing and device reinitialization. They are not
   implied by battery reporting or brightness control.


## Read-only preflight and SMBus protocol engine

The next v18 desktop trial adds `srvros.laptop-services=probe`. It first requires
GPU `8086:1626`, LPC `8086:9cc3`, the exact captured DSDT identity, and validated
ECDT/FADT checksums, lengths and resources. Unmatched systems make no register
accesses. Host tests exercise rejected tables and also parse the actual captured
firmware files.

The ECDT independently confirms EC data/status ports `0x62`/`0x66`, UID 0 and GPE
`0x4e`. The FADT gives PM1 control `0x1804` (two bytes), GPE0 base `0x1880` (32
bytes), SCI 9 and SMI command `0xb2`. The extended GPE GAS width is zero in this
firmware; the legacy block length establishes the size. The probe reads PM1
control, EC status (not EC data), and the EC GPE enable byte. It does not enable
ACPI, clear status, issue a query or consume an EC response. These readings will
show what state the native boot inherits and guide the AML/SCI ownership decision.

The display preflight maps two read-only, uncached register pages only when PCI
memory access is already enabled. It records CPU PWM control/duty and PCH PWM
control/period/duty, then decodes valid CPU-controlled or PCH override mode.
It does not switch modes, enable a disabled backlight or write duty. Register
names/offsets are checked against the pinned MIT-licensed
[Intel register definitions](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_backlight_regs.h).
The actual mode must be observed before implementing a brightness writer.

The independent `ec_smbus.c` read-word state machine now implements the EC SMBus
protocol over the earlier transport callbacks: idle verification, address/command,
protocol 9, paced completion polling, status and little-endian result. EC-byte
and overall deadlines, overlapping-operation refusal and poisoning on uncertain
completion are tested. It still has **no live hardware binding**. The caller must
own the EC and SMBus bank and serialize AML/query traffic. It writes request
registers only; it does not write battery or charger parameters.

Battery/AC reporting and brightness controls remain unavailable. The remaining
work is firmware initialization/event integration, a scheduled cached SBS
provider, and a mode-preserving backlight provider followed by their UI controls.
The new USB boot/session logs make the next physical preflight recoverable without
relying on a short photograph window.


## Physical preflight recovered after automatic return

Release **8d348e2935c28972** produced boot and exit snapshots on the real USB
volume, recovered and hash-verified after the user returned to Ubuntu. The saved
exit snapshot includes the expected release and diagnostics-collected marker;
no error markers were found. It ends before the final flush/unmount/reset, so the
file alone does not independently record those later steps. The automatic code
path requests reset only after successful save and unmount. No desktop was opened
and physical input acceptance remains pending.

| Reading | Observed value |
| --- | --- |
| PM1 control / SCI_EN | `0x0000` / disabled |
| EC status / EC GPE enable | `0x00` / disabled |
| CPU PWM control / duty | `0x00000000` / `0x00000000` |
| PCH PWM control | `0xc0000000` (enabled, PCH override) |
| PCH period/duty | `0x0ad90184`: period 2777, duty 388 |

The current duty is about 14% of the PWM period; that is a register ratio, not
perceptual brightness. This resolves the backlight control-path question for this
boot. Brightness bring-up should preserve the current mode, period, polarity and
reserved bits, expose a bounded nonzero range, verify readback and stop on changed
controller state. Initializing the desktop must not itself change panel brightness.
Do not switch to CPU PWM or infer a different panel's register programming.

For battery/AC, SCI_EN=0 means the native boot has not entered ACPI mode. The EC
status byte being clear does not prove exclusive ownership or initialized SMBus.
Resolve ACPI namespace initialization, SCI/query dispatch and EC serialization
before binding the read-word engine. The evidence favors implementing the needed
AML/SCI integration rather than treating the register inventory as a battery driver.

Collected evidence: `build/desktop-collections/collection-daf5ezhd/`, also retained
under `build/native-desktop-trials/8d348e2935c28972/physical-collection/`.


## Brightness backend and Settings control

The first backend uses the captured PCH override state only: CPU PWM control 0,
PCH control `0xc0000000`, period 2777 and a valid nonzero duty. Exact firmware,
chipset and ACPI-resource validation precede mapping. The opt-in v20 desktop
profile makes the PCH register page writable but performs no initialization write.
The callback-based backend exposes query and bounded 5–100% set operations;
changing duty preserves the period, enable, polarity and mode. Each actual write
is read back. Unexpected mode/period/duty or readback disagreement disables further
access instead of attempting speculative recovery. No EC/AML operations are added.

The new compositor-only brightness syscall is bounded and allowed during storage
service windows. Protocol v8 carries serial-tagged requests/replies between
Settings and the compositor. Settings refreshes on focus, rejects stale responses,
times out unavailable replies, preserves slider capture during acknowledgements,
and disables the control on unsupported hardware. Brightness is session-only;
there is no automatic restore from disk and no new suspend/lid policy.

Register definitions were checked against the pinned MIT-licensed Intel header
linked above. This implementation does not copy Linux's brightness-mode-switching
code. Battery/AC reporting still requires ACPI initialization and event handling.

## Isolated AML evaluation and SBS sampling foundation

The next device session vendors MIT-licensed uACPI 7.0.0 unchanged, with a narrow
SrvrOS adapter. See [the port boundaries](../ports/srvros/uacpi/PORT.md).
`srvros.aml=probe` evaluates firmware in private copies: ACPI tables and permitted
ACPI NVS/reclaimable regions are copied, PCI configuration writes affect a private
snapshot, and System I/O and arbitrary physical memory are denied. It does not
enter ACPI mode, initialize device namespaces, call EC `_REG`, enable SCI/GPEs or
access EC data ports. It is discovery evidence, not an active battery driver.

The complete captured DSDT plus eight SSDTs now load and evaluate the target EC
resources, SMBus presence/configuration and SBS declaration successfully in the
host adapter. Explicit GNVS, PNVS and PCI snapshots supply the firmware inputs.
The recorded run executed 3,566 AML operations, made four private snapshot
writes, reported no denied accesses/errors and released all interpreter
allocations. Synthetic namespace tests and native QEMU probe tests cover the
adapter separately. The exact laptop-native probe remains a hardware gate.

`sbs_sample.c` adds a host-tested read-word sampler over the existing EC SMBus
engine. Fifteen words bracket capacity mode, scaling and status around the
measurement. A mode/scale change rejects the sample. Failed transactions retain
last-good values with an invalid-current flag and timestamp; partial samples
are never published. Signed current, voltage, charge/energy capacity units,
unknown time estimates, reported state of charge and gauge initialization/error
state remain distinct. Tests include delayed transport, cancellation, timeout,
clock rollback, stale samples and scaling limits. No live caller is bound yet.

The next battery milestone is still ACPI mode/SCI/query ownership and serialized
EC access, followed by a scheduled cached provider. Do not bypass that boundary
by reading the bank directly from the desktop. References are the official
[SBS data specification](https://sbs-forum.org/specs/sbdat110.pdf) and its
[1.1 errata](https://sbs-forum.org/specs/errata_sbdat110a.pdf).

## Event-query transport prerequisite

`ec_transport_begin_query` adds the ACPI QR_EC exchange to the existing bounded
byte transport. Once the input buffer is available, it checks SCI_EVT and sends
`0x84` only when an event is pending. It then waits for one output byte without
writing an address. A cleared event before submission or a zero reply completes
as “no event”; clearing SCI_EVT after submission does not discard the pending
reply. Existing timeout, stale-output, cancellation and poisoned-transport rules
still apply. This follows [ACPI 6.5 §12.3.5 and §12.6.2](https://uefi.org/specs/ACPI/6.5/12_Embedded_Controller_Interface_Specification.html#query-embedded-controller-qr-ec-0x84).

The query operation does not acquire ownership, drain an event queue, acknowledge
a GPE or run `_Qxx` methods. The future runtime must serialize it with AML access
and the complete SMBus register-bank transaction, then dispatch returned events
outside that ownership scope. No live caller or new port access is enabled.

Host tests cover all 256 reply values, delayed IBF/OBF, event clearing, ordinary
read reuse, overlapping requests, stale output, timeout and cancellation before
a late reply. Existing SMBus and SBS tests plus transport/SMBus ASan/UBSan runs
passed. Production compilation passed; the rebuilt kernel's PT_LOAD segments
match the staged **fe119d1c99070b18** image exactly because the unbound transport
is removed from the native image by section garbage collection. The physical
candidate therefore remains the validated audio/device build. Evidence is in
`build/review/ec-query-*`.

## Live ACPI runtime integration plan

Review of the actual kernel and pinned uACPI 7.0 sources identifies these next
implementation blocks. They are prerequisites, not a claim that live battery
sampling is enabled.

1. **Interrupt lifecycle.** CPU0 shared handler registration/removal and masked
   IO-APIC preparation/readback are now implemented and host-tested. Legacy
   timer/keyboard/serial/mouse dispatch stays reserved; e1000 still receives its
   existing callback. `irq_disable_line` changes only the legacy PIC mask.
   The new route APIs do not establish source ownership. Resolve the FADT SCI through the
   platform interrupt topology, retain polarity/trigger information, and reject
   unsupported controllers/routes. SCI must not inherit the default ISA edge
   policy. Test masked installation, repeated level assertion, shared handlers,
   failed setup and removal before exposing the uACPI IRQ callbacks. Never print
   an unbounded stream of unhandled SCI messages from interrupt context.
2. **Scheduler adapter.** Stable nonzero kernel-thread lifetime identities and
   CPU0 counting semaphores with zero/finite/infinite waits, IRQ-safe signaling
   and closed-state teardown now pass host and real worker VM checks. Bind them
   to the live adapter along with locks that restore prior interrupt state.
   The existing wait queues provide a starting point; the isolated probe's
   single-threaded callbacks do not satisfy this runtime contract. Test wakeup
   races, timeout rounding, overflow and shutdown before binding them. Start AML
   work as an ordinary scheduler worker. Admission during storage ownership
   needs a separate dependency audit; an AML worker must not be declared an
   I/O service merely to reduce latency.
3. **Deferred work.** `runtime/work_queue.c` and `work_worker.c` now provide
   allocation-free enqueue/dequeue and a CPU0 worker with drain/stop semantics. The pinned API explicitly allows
   callbacks to reschedule or free their own work item, so a detached invocation
   is essential. The worker executes both GPE and notification work outside
   queue locks and yields after every callback. Completion waits must synchronize in-flight interrupts
   first, then pending/in-flight work, including work scheduled by callbacks.
   Host and synthetic-IRQ VM tests now cover this worker and teardown; hardware
   level assertion and actual uACPI callback binding remain separate checks.
4. **Separate live adapter and initialization.** A separate full-runtime Q35 VM
   kernel now passes SCI, deferred AML mutex/sleep, failed-install rollback and
   reset tests; normal hardware staging still uses the restricted probe. The native interpreter
   is compiled with `UACPI_REDUCED_HARDWARE`; changing its runtime flags cannot
   restore the omitted event machinery. Use a separate full-hardware build
   configuration/adapter, initially mutually exclusive with the isolated probe,
   rather than linking two copies with colliding global symbols. The separate VM
   target now implements this build separation; it is not a native provider. Keep both build
   paths tested. In the pinned sources `uacpi_initialize` normally enters ACPI
   mode, `uacpi_namespace_load` installs event handling, namespace initialization
   runs `_REG`/`_STA`/`_INI`, and finalizing GPEs enables AML event users. Therefore
   use an explicit staged initialization policy and `NO_ACPI_MODE` until SCI and
   EC prerequisites are ready. That flag is not a read-only guarantee: full
   initialization also touches event registers. Preserve failure diagnostics and
   mask/disable owned event sources before teardown; do not free a callback's
   state while interrupts or work can still reference it.
5. **EC and battery ownership.** Install the EC address-space/query path before
   firmware methods need it. Resolve the Global Lock and `_OSI` policy from the
   captured firmware; the probe's Darwin selection is not yet a native policy.
   Serialize AML EC access, queries and each complete SMBus bank transaction.
   Dispatch `_Qxx` after releasing the EC transaction owner, so methods that use
   EC/SMBus cannot deadlock it. Bound query draining and expose pending, timeout,
   stale and poisoned states. Then bind the existing SBS sampler and publish a
   versioned cached power snapshot; the desktop only reads that snapshot.

Acceptance should proceed from host fault injection to a synthetic QEMU
SCI/EC fixture, then an opt-in laptop runtime trial. The synthetic fixture must
cover query storms, delayed replies, `_REG` access, self-rescheduling work,
teardown and failures during initialization. The laptop round should check AC
connect/disconnect, fresh battery values, input under file I/O, and saved event
counters. Lid policy and suspend remain separate milestones. The installed
**fe119d1c99070b18** candidate enables none of this new live runtime machinery.

The implementation contracts above come from the vendored upstream
[`kernel_api.h`](../ports/upstream/uacpi/include/uacpi/kernel_api.h),
[`uacpi.c`](../ports/upstream/uacpi/source/uacpi.c) and
[`event.c`](../ports/upstream/uacpi/source/event.c), alongside SrvrOS's
[`irq.c`](../kernel/src/arch/x86_64/irq.c) and
[`ioapic.c`](../kernel/src/arch/x86_64/ioapic.c).
