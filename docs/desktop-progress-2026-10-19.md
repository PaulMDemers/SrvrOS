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
separate profile and normal hyphenated exFAT UUID arguments. Native release `1fe693c9889f131f` (source `b3f0e31`) subsequently completed
59,988,181 us with 6,000 timer samples, 5,673 in idle (94.55%). The saved session
`session-00065e29979b3f87-00` contains all four logs with no attention flags;
package temperature fell from 80 C to 75 C and the final fan reading was
1,206 RPM. All 63 SMC samples completed without errors. The machine returned
to Ubuntu, the one-shot entry was consumed and an offline read-only exFAT check
was clean. This is a console sample after prior host activity, not an equilibrium
temperature or power measurement, and it does not accept desktop idle behavior.

## Heap validation exposed by firmware replay

Replaying captured DSDT/SSDT tables alongside QEMU firmware reproduced a halt:
`heap: corrupt cycle where=kmalloc-exit`. The heap checker stopped after 4,096
blocks even when the address-sorted list was valid. Its existing strict
ascending `next` check already rejects cycles, so the arbitrary count ceiling
has been removed. This is a demonstrated replay failure; the original native
stall is not yet confirmed to have the same cause.

The allocator's 40-byte metadata header also broke its intended 16-byte payload
alignment. The header is now aligned/padded to 48 bytes. Allocation requests
which would overflow size rounding/header/page arithmetic are rejected before
changing heap state. A regression uses the actual allocator with 6,000 live
allocations, fragmented reuse, zeroed allocations, large allocations, exhaustion,
overflow and a genuine corrupt cycle. Normal and ASan/UBSan runs pass, as does
the complete core suite. The captured-table replay now returns in about 2.1 s
with no interpreter allocations left. QEMU firmware/uncaptured regions produce
errors in that replay, so it is not a complete or physically faithful firmware
result. The synthetic native AML fixture passes without errors/denials/leaks.

## Native isolated AML completion

Release `4e5f438c4069752b` (source `17c6ed3`) completed on the Air7,2 and
returned to Ubuntu. The matching session `session-00065e29c90512da-00`
preserves a clean exit: 5,125,724 us, zero interpreter errors/denials, four
private snapshot writes and zero remaining interpreter allocations. Namespace
loading took about 3.59 seconds. EC resources resolved to ports 0x62/0x66, GPE
0x4e, SMBus `_EC` 0x2010 and one SBS battery declaration. The FADT revision
warning still appears, followed immediately by successful initialization.

An offline read-only exFAT check was clean (161 directories, 805 files). This
clears the isolated native probe's stall gate after the heap fixes; it does not
prove which heap defect caused the original screenshot's stall. Ordinary desktop
and idle profiles still omit the diagnostic. No ACPI mode/SCI/EC ownership,
live battery transaction, lid policy or suspend behavior is accepted here.

## Synchronization for future live ACPI

CPU0 counting semaphores now provide nonblocking, finite and infinite waits,
IRQ-safe signal, close/wakeup and drained teardown. Deadline checks use the
monotonic clock with upward timer rounding. They are for ordinary kernel
workers; cancellable userspace and SMP need additional lifetime/locking work.
Scheduler workers receive lifetime IDs which are not reused with their slots;
spawn publication is serialized against local preemption. These changes do not
bind or initialize a live ACPI adapter.

Host tests pass signal/accounting, the check-to-sleep race, spurious wakeups,
timeout, shutdown, counter overflow and unavailable-clock/scheduler cases. A
VM ran the actual `semaphoretest` four times with distinct worker IDs, blocked
signal/close wakeups, no premature timeout and drained teardown.
The complete threadstress VM suite also passes after the scheduler changes.

## Shared IRQ lifecycle foundation

Added CPU0 shared IRQ registration/removal with unique lifetime tokens,
reserved-line and duplicate refusal, and dispatch-time mutation exclusion.
I/O APIC masked preparation now validates the installed vector range, rejects
live/pending routes, specifies polarity/trigger mode, and checks readback.
Existing legacy route programming masks before changing destination; no new
source is enabled. Unhandled IRQ messages are bounded to the first occurrence.
Host registry/routing fault tests and the e1000 desktop/System VM regression
passed. SCI topology ownership, level-trigger fixture testing and runtime
adapter teardown remain; this is not a live SCI or battery acceptance.

## Deferred-work worker

The ACPI queue now has an ordinary CPU0 scheduler worker, allocation-free IRQ
admission, finite drain waits and idle-only stop/restart. Callbacks can reschedule
or free their own work item; the worker yields after each invocation. Failed
stop waits never authorize freeing live state. Ordinary startup starts none of
this machinery and the isolated probe callbacks remain restricted.

Host faults and ASan/UBSan passed. The VM exercised four distinct worker
lifetimes, each with eight actual software-interrupt entries, duplicate pending
refusal, a blocked callback and drain timeout, 1,000 self-reschedules, self-free
and successful stop. This tests the ISR/worker path, not hardware SCI assertion.
Live uACPI completion still needs interrupt-producer synchronization before drain.

## Real emulated SCI lifecycle

The Q35-only SCI fixture passed three lifetimes and 24 power events. Every event
was intentionally left asserted for its first EOI and delivered a second IRQ,
then deferred work cleared and re-enabled it. Shared dispatch, failed-start
cleanup after mode entry, restored masked routing and no post-stop callbacks
passed. Host profile/refusal and masked restoration checks passed. This fixture
runs no AML and enables nothing on native laptop profiles; the full uACPI
adapter and EC ownership still need integration.

## Full event-enabled uACPI in a separate VM build

The alternate `build/acpi-runtime/srvros.elf` now binds the upstream full runtime
to the tested IRQ/semaphore/thread/worker primitives. Normal builds retain the
restricted probe, and hardware staging continues to consume the normal kernel.
Only Q35's validated PM/GPE/SMI ranges and inventoried firmware tables are exposed;
PCI/MMIO operation regions remain denied. Namespace initialization and EC/GPE
method activation are not performed.

The final VM passed three lifetimes, each with failed-install cleanup, ten SCI
power events and competing AML mutex/sleep evaluations. A stop with pending work
drained before namespace reset; all clean lifetimes ended with zero errors,
denials and interpreter allocations. A deliberately wedged worker then caused
the expected teardown halt retaining live allocations. This verifies failure
containment in the VM and does not authorize native battery/EC activation.

## Synthetic EC and battery pipeline

The VM now connects real uACPI EmbeddedControl handlers to a modeled controller
using the existing EC/SMBus/SBS engines. Two full lifetimes passed `_REG` I/O,
foreign-caller refusal, complete SBS samples, queries deferred during samples,
35 query methods and 18 notifications per lifetime, and timeout quarantine with
last-good retention. A third delayed-setup teardown also passed. All clean
resets detached their handler and left zero interpreter allocations.

An initial reset failed when generic namespace destruction removed names needed
by `_REG` disconnect. The adapter now explicitly disconnects while the namespace
is intact; the failure evidence is retained. No physical EC register is exposed
by this work, and the sample values remain synthetic. The full-runtime SCI,
mutex-contention and failed-drain suite passed after integration.

## Explicit integer desktop scale

Added `deskd --scale=2` with a bounded scaled-blit syscall. Coordinates, window
surfaces and damage stay logical; the kernel expands directly into the physical
framebuffer. The existing eight-window caches do not grow. Default remains 1×;
this is integer raster enlargement, not native-density glyph rasterization.
Saved BMPs retain logical dimensions and remain within Preview's input budget.

The full host suite passed, including real syscall validation/chunk-copy/error
cleanup and guarded padded-framebuffer tests for clipping, color layout and
invalid arithmetic. UEFI VM trials passed at 2880×1800/2× and 1440×900/1×:
mixed-case text persisted exactly, titlebar maximize/restore matched logical
geometry, every physical 2×2 block matched, and saved logical BMPs expanded to
exact screen pixels. Both exited cleanly. Visual inspection confirmed the
maximized Retina layout. BIOS initially fell back to 1024×768; the harness now
uses UEFI and asserts actual mode selection. A subsequent harness attempt left
unsaved text open; it now saves and verifies that document before requesting exit.
Neither failure was counted as a pass. This does not accept physical Pro GPUs,
firmware mode selection, external monitors or Retina input calibration.

Evidence: `build/review/desktop-scaling-{2880x1800-2,1440x900-1}/` and the
`scaling-*` records under `build/overnight-20261018/`.

## Unattended desktop idle capture and serial-record correction

Source `e6fd681` adds a read-only CPU0 scheduler snapshot ABI and separate v25
`--desktop-idle-capture` profile. It waits for Text's first committed frame and
startup settling, measures 60 seconds, saves a workspace screenshot, then exits.
The kernel returns only after successful process completion, log save and unmount.
Ordinary desktop trials retain manual exit. Host syscall checks, desktop tests
and 65 hardware-tool tests passed. Exact-stage VM release `06c646455f08555e`
completed 59.96 seconds/6,000 timer samples, 98.27% idle, one damaged-frame
presentation, and an offline-decodable saved BMP before a clean reset. Missing
workspace halted as intended. This is VM evidence; native acceptance is pending.

The separate 30-minute frozen ACPI soak stopped after 21 complete runtime/EC
rounds (42 successful suite runs), at about 20 minutes. Preserve this as a failed
soak: its next runtime harness matched `elapsed_us=2` from an unfinished serial
line, before the rest of the numeric value arrived. The guest had reported
`race=PASS value=10 waits=1`; the host compared a truncated time to the minimum.
All three ACPI harnesses now match only complete lines. Unit tests exercise every
byte split of single- and multi-line numeric records, including a partial halt
record. Runtime, EC and shared-SCI VM suites pass again with this reader. A new
full soak remains required; the interrupted soak is not promoted to a pass.

The failed immutable bundle is retained outside `build` under
`~/.local/share/srvros/vm-soaks/20261019-runtime-3099e1fe/rounds/0021/runtime/`.
No native SCI/EC service was enabled by either the test or its collector fix.

## Native desktop idle result

Release `06c646455f08555e` returned automatically. The new session
`session-00065e2b0ab4fabc-00` contains all four verified logs, no attention
markers, completed measurement/screenshot and clean desktop-return markers.
The 59,998,087 μs measurement has 6,000 timer samples, 5,649 idle (94.15%),
24,559 switches and 3,000 preemptions. Only one frame and 76,384 framebuffer
bytes were presented during the sample. These are CPU0 scheduler/render
measurements, not package energy or interactive responsiveness acceptance.

The saved 1440×900 BMP was copied, hash-checked, decoded and visually reviewed:
Text, global menu, dock and test notice were intact. Final temperature was 74 C
and fan 1,223 RPM, with 81 SMC samples and no errors. Offline read-only exFAT
check was clean (162 directories, 810 files). Ubuntu remains default with no
one-shot armed, and accepted desktop `b427b78f21644bd2` remains previous.
Evidence: `build/native-desktop-trials/06c646455f08555e/physical-after/`.
The automated physical window is now closed; remaining work is host/VM only.

## Read-only package energy and active-frequency groundwork

Added an explicit `srvros.power=read-only` diagnostic provider for the observed
Broadwell-U CPU. It samples package energy and CPU0 APERF/MPERF once per second,
with independent validity, bounded interval/arithmetic checks and stale expiry.
The `power` monitor command and workspace collector expose cached results.
Ordinary boot stays disabled, and unsupported CPUs/hypervisors issue no provider
MSR reads. No frequency, idle-state, fan or power-limit controls were changed.
See [scope, gates and Intel primary references](power-telemetry.md).

Full host tests/build passed, along with ASan/UBSan arithmetic tests, six desktop
collector tests and both VM boots: default disabled and explicit opt-in refusing
an emulated Broadwell CPU. The Linux read-only fixture decodes to 6,010 mW package
power and 1,424 MHz active frequency during development load; this is not SrvrOS
energy acceptance. Physical enablement requires a future explicitly staged trial.
The physical reboot window has closed, so this change was not installed.

The corrected ACPI soak has restarted in an independent immutable bundle from
`99a1d728`; its final result remains pending. The earlier failed bundle is retained.

## Staged power-capture preparation

The new explicit v26 profile adds power telemetry only to a desktop idle capture;
older profile recipes remain immutable. Desktop logs now identify the measurement
window. The collector derives energy and active-frequency averages only from
valid checkpoints inside that window, reports the actual covered duration and
excludes screenshot-writing activity. Missing/refused/stale/inconsistent counters
produce no power result. Host tests cover rollover, duplicate/conflicting samples,
resets, stale readings and coverage/completion requirements.

The exact-stage VM must report power unavailable with zero samples while checking
the ordinary desktop screenshot/save/unmount/return workflow. This deliberately
separates successful capture mechanics from power acceptance. The profile is
being prepared for a later physical window; nothing is installed or armed.


V26 release `e03c015569fd2283`, source `854b737`, passed exact-stage UEFI/GRUB
rehearsal: complete desktop sample, saved/decoded BMP, clean exFAT and reset.
Power was correctly unavailable with zero samples in the VM; the collector
returned no power measurement. Missing-workspace rehearsal halted without reset.
All 68 hardware-tool tests and desktop/workspace host tests passed. Evidence and
an explicitly **not installed/not armed** record are saved under
`build/native-desktop-trials/e03c015569fd2283/`.

## Client control delivery deadlines

A new host regression reproduced a second queue-age failure: after the compositor
paused for more than five seconds, it killed a healthy client before attempting
to deliver its pending surface release. Configure/close requests had the same
attribution problem. This is a reproduced independent failure, not proof that it
caused the earlier physical System-window closure.

Control packets now get a delivery attempt first. Repeated blocked sends have a
bounded five-second deadline, and the client's response deadline starts at
successful delivery. Control traffic resets an interrupted input-delivery wait.
Forced close is sent once; a client that ignores it is still killed after the
response deadline. The host regression verifies delayed release/configure/close,
recovery after backpressure, control/input interleaving and genuinely stuck
clients. The original failing evidence remains in
`build/overnight-20261018/control-timeout-reproduction.txt`.

Full desktop host tests/build and the live System/network VM passed. A separate
three-session GUI soak passed 36 app lifetimes with repeated arranging, loading,
searching and closing. Heap use remained 4,096 bytes after each session; physical
frame use plateaued exactly after the first. No GUI workers remained. The soak
now explicitly rejects unexpected client/input/control timeout closures instead
of counting retirement alone as success. Physical acceptance is still pending.

## Isolated VM serial endpoints

The second full ACPI soak stopped after 26 complete runtime/EC rounds plus one
runtime suite (53 completed suites), at 1,438.69 seconds. The next QEMU never
started its guest: its stderr reports the selected TCP serial port was already
in use. The harness then timed out connecting to that VM's absent QMP endpoint.
This is preserved as a failed soak, not a kernel failure or a completed run, in
`~/.local/share/srvros/vm-soaks/20261019-runtime-99a1d728/rounds/0026/ec/`.

The runtime, EC and SCI fixtures now use a private Unix serial socket in each
fixture's temporary directory, avoiding the free-TCP-port selection race. A
bounded connection helper has real-socket tests for delayed listening, unrelated
endpoint isolation, missing endpoints and invalid timeouts. All three VM suites
passed with private serial endpoints. The next immutable soak manifest hashes
all copied tool/fixture/boot dependencies as well as its selected kernel. A fresh
full-duration soak remains required. No native ACPI runtime is enabled.


## EC disconnect ownership and failure containment

Extending the modeled EC's `_REG` disconnect to sleep and perform EC I/O
reproduced a lifecycle error: removal ran on the monitor thread, outside the
controller's owning worker. The access was refused. The pinned interpreter
logged the AML error but still returned success from handler removal; the old
adapter then freed the namespace. Both clean-controller and poisoned-controller
reproductions are preserved under `build/overnight-20261018/`.

The fixture now closes external admission before draining existing work. It
removes notifications on the lifecycle caller (upstream waits for deferred work),
then schedules address-space removal on the owning worker. A finite drain and
explicit read/write disconnect handshake gate namespace destruction. The VM
passed two clean sample/query lifetimes plus teardown during delayed setup,
with sleeping disconnect I/O and zero remaining allocations. A fourth lifetime
verified last-good sample retention after a transport stall, retry refusal and
an intentional teardown halt retaining interpreter state. This is an
expected negative VM test, not a native failure or battery acceptance.

A second negative variant leaves disconnect AML sleeping past the five-second
drain deadline and requires a halt before namespace reset. Its first attempt
correctly exposed the interpreter's two-second cap on a single Sleep, so the
fixture now uses seven consecutive one-second sleeps. A loop-based attempt
hit the adapter's one-second AML loop guard first, as intended by that guard.
Both early attempts remain local evidence and are not counted as worker-deadline
passes. Neither interpreter guard was weakened.

Worker/queue, EC, SMBus and SBS host regressions and the full runtime SCI/mutex
regression passed. No native port gate or
ordinary boot profile changed. General firmware still needs explicit ownership,
initialization and error propagation policy; the synthetic handshake cannot be
assumed for arbitrary AML.


## Completed ACPI runtime soak

The immutable `6b7c2bc8` artifact bundle completed 1,821.58 seconds: 37 full
runtime/EC rounds, 74 successful suite executions. After completion, every frozen
input hash and all 74 suite logs were verified. The bundle is retained outside
`build` at `~/.local/share/srvros/vm-soaks/20261019-runtime-6b7c2bc8/`.
Its normal lifetimes require zero errors/denials/leaks; intentionally wedged
teardown is an expected negative case. The two earlier incomplete soaks remain
failures and are not included in this pass. This accepts the tested VM artifact,
not native ACPI or the later `81e523d` disconnect extension.


## Worker-owned namespace initialization

The separate VM adapter now tests namespace initialization after connecting the
modeled EC on its owning worker. An explicit `start-fixture` selects a validated
synthetic DSDT only for the interpreter; generated tables and I/O permissions
are unchanged. After EC connection, root and system-bus initialization, `_STA`
and sleeping `_INI` are checked before readiness. Two full lifetimes then exercise
queries, notifications, SBS sampling and clean disconnect; poison containment
remains a negative case.

The first attempt against QEMU's normal DSDT was refused because its HPET and
hotplug methods require regions outside the allowlist. This exposed another
important API distinction: namespace initialization can return success while
logging method failures. Error/denial deltas now make the fixture refuse readiness
in that case. The negative VM requires no ready service, refused sampling and
clean teardown. The positive test uses our small synthetic namespace; no register
permissions were widened to make unrelated firmware initialize. Native early-EC
binding, firmware initialization policy and AML GPE finalization remain open.

The positive initialization, generated-namespace refusal, initialized disconnect
deadline, ordinary EC and full SCI/mutex regression suites passed. The normal
restricted AML probe was tested separately to preserve build isolation.


## Trackpad faults cancel interaction without discarding typing

A transport regression reproduced a faulty distinction: the driver converted a
failed held trackpad press into an ordinary button release. Palm suppression did
the same. A widget that activates on release could therefore treat interrupted
input as a click. This is a separately identified risk, not proof of the cause
of an earlier physical window closure.

Transport faults and held-press palm suppression now emit `MOUSE_CANCEL`, discard
uncertain queued pointer actions and preserve an external mouse's physical button
state. The gesture engine still refuses rearming until physical release. A
second host regression reproduced that the compositor's cancellation path erased
queued keyboard/text packets as well as pointer input. It now removes only
pointer/scroll packets and keeps one pending cancellation outside the bounded
input queue. Cancellation uses the existing bounded control-delivery path, so
even a full 64-packet keyboard queue can recover without losing typing or closing
a healthy client. Duplicate cancellation coalesces.

Host cases cover endpoint failure, palm interruption, held-press suppression and
fresh release, external button ownership, queued-motion cancellation, all 64
key/text packets delivered in order after backpressure, and wrapped mixed queues.
The original assertion failures are retained in `pointer-cancel-reproduction.txt`
and `pointer-cancel-keyboard-reproduction.txt` under the overnight evidence folder.

Full core/desktop host checks and the normal OS build passed. VM context-menu
and System/network regressions passed. These checks preserve the distinction
between injected fault coverage, normal VM interaction and pending physical
trackpad acceptance.


The expanded immutable ACPI bundle `42c64df7` also completed its full duration:
1,811.90 seconds, 20 rounds and 100 successful suite executions. Each round
covered runtime SCI/mutex behavior, ordinary EC, initialized synthetic namespace,
initialized disconnect timeout, and generated-namespace denial. All frozen input
hashes and 100 logs were verified after completion. Expected poisoned/timeout
halts remain negative containment cases. Evidence is retained outside `build`
at `~/.local/share/srvros/vm-soaks/20261019-runtime-42c64df7/`. This is VM ACPI
acceptance for that artifact; the later pointer fix and native firmware require
their own evidence.

## Portable Linux CPU policy inventory

The read-only collector now saves `cpu-policy.json`: CPU topology/online sets,
idle driver/governor and per-CPU states, and frequency policy membership,
governor, limits and available feedback. It preserves raw units and distinguishes
absent optional attributes, failed reads and unknown directory availability.
An online-set recheck flags observed hotplug without claiming atomicity. It does
not read MSRs or write CPU controls, and does not infer SrvrOS device support.

Ten inventory fixture tests passed, including six CPU-policy cases for grouped
policies, numeric ordering, allowlist preservation, absent subsystems, denied
reads/enumeration, disappearing states/fields and changed online membership.
The tests now have a `test-linux-inventory` Make target included in `test-core`.
A complete fresh Linux capture had zero partial reads; raw evidence stays under
`build/hardware/20261019-cpu-policy/`. Four logical CPUs use `intel_idle`/`menu`
and `intel_cpufreq`/`schedutil` with 500–3,200 MHz policy limits. CPU0 exposes
POLL through C10. This records Linux policy, not measured residency or energy.
Installed payloads and recovery were unchanged; no physical boot was requested.

## Public repository readiness

The README is now a short project introduction with a Linux/QEMU quick start,
current desktop screenshot and explicit support boundaries. Current architecture,
status, roadmap and documentation-index pages replace the mixed historical
inventories; the original overviews are retained under `docs/history/`.
Contribution/security guidance, issue/PR templates and a third-party notice index
make the repository easier to navigate and contribute to. Dated test journals
remain historical evidence.

`tools/desktop_gallery.py` opens real Text, Files, Settings and Preview apps in a
disposable 1440×900 QEMU guest, using synthetic documents and the existing color
fixture. Three unretouched PNGs were visually inspected. Its manifest records
source/payload hashes, fixture/generator identity, environment and image hashes.
A clean desktop exit and lossless pixel conversion are checked before publication.
The final gallery uses source `d9f5ce2`; later test/docs changes do not alter the
pictured kernel/userspace. The fixed guest clock is labeled as a fixture.

`make check-docs` checks local inline Markdown destinations and gallery hashes/
dimensions, including in CI. It does not check remote URLs or heading anchors.
The local check passed 340 file destinations, and a deliberate missing-link
fixture was rejected. A limited tracked-working-tree credential-pattern scan
found no matches; this was not a Git-history or comprehensive security audit.

The remote clean build exposed a missing compiler dependency for native ACPI
objects: parallel compilation could start before Zig's initial download completed.
Commit `d9f5ce2` fixes the Make dependency and adds an isolated delayed-bootstrap
regression across the three ACPI object families. The next remote run built both
images successfully, then exposed a GCC 13 diagnostic in the scheduler-status
host fixture. Commit `57ae640` keeps the mock VMM boundary out of line, as in the
real separately compiled kernel; kernel code and warning flags are unchanged.
The warning was reproduced with a locally unpacked GCC 13 and the corrected
fixture passed with GCC 13 and GCC 15. The symlink-based bootstrap fixture skips
Windows rather than requiring privileged symlink creation there.

Local validation passed: normal OS build, core/desktop host tests, a forced host
test rebuild with GCC 13, the full baseline smoke suite (including all 481 CLI
commands, shell editing, syscall ABI, UEFI USB input and legacy displayd), and
the new three-image gallery with clean exit. Current GitHub results must be read
from the matching commit; a local pass is not a remote CI pass. No physical boot,
installation, firmware write or acceptance upgrade was part of this cleanup.

## Interactive hardware return and Terminal backspace correction

The user-authorized v22 round `fed370ffbe33d554` used source `c6e50ec`,
whose tree matches merged baseline `e2d172b`. The exact-stage two-boot
UEFI desktop/audio rehearsal passed before installation. Recovery remained
`b427b78f21644bd2`, with Ubuntu the normal default.

After return, session `session-00065e37230cc95f-00` matched the staged release
and was absent from the preflight session list. All four logs were copied and
hash-verified, with a saved exit snapshot, clean desktop process exit and no
collector attention flags. Two BMP screenshots were copied and hash-verified.
The user reported generally good behavior but intermittent stray Terminal
characters after backspacing; the first screenshot shows a slash to the right
of an otherwise cleared prompt. The second shows System Sensors with fresh
67 C CPU readings and 1,209 RPM. Logs show completed PCM playback of 1,152,000
bytes with zero underruns, a completed tone test, brightness changes, and no
System/Help/Terminal failure at exit. This is not individual confirmation of
headphone routing, audible channel separation or every secondary-click gesture;
the final secondary-tap counter was zero. Offline read-only exFAT checking was
clean (163 directories, 818 files). The one-shot selection was consumed.

The Terminal artifact reproduced in both the actual linenoise editor feeding
the terminal cell model and the desktop VM. After shortening a line, the editor
writes spaces to erase its old tail but used to return without restoring the
cursor when the edit position equaled the new end. Its fast append then wrote
past the remembered extent, leaving a slash that later refreshes did not erase.
The correction also restores the cursor after that tail-clearing case, while
retaining the fast path for unchanged/growing lines. This is an editor output
position bug, not missing compositor repaint damage.

Host regressions check both returned commands and displayed cells after repeated
backspace, delete, Ctrl-K/U/W, shorter history recall and appended slashes.
The desktop harness compares the actual erased input-row pixels. Saved old-build
captures fail that check; the first corrected-build comparison also included the
blinking caret because the fixture prompt is now `/fat/home $ `. The comparison
now checks the empty region after that caret, asserts the initial geometry, and
still rejects the saved old-build slash. Raw before/after evidence is local under
`build/native-desktop-trials/fed370ffbe33d554/`.

The correction is not in the installed hardware payload; a physical backspace
retest remains open. No new boot is armed and the unattended automation remains
stopped.

Final validation passed: `make -j2 all test-core test-desktop`, the full native
Terminal VM (including exact pixel clearing, clipboard, two sessions, Ctrl-C,
resize, close cancellation and saved output), the console shell-edit smoke test,
and `make check-docs`. The terminal harness's old `20 90` resize expectation was
also corrected to `20 94`: its 1000×550 client, 54×98 terminal insets and current
10×22 cells yield 94 columns and 20 rows. No production geometry changed.


## Terminal correction physically accepted

Follow-up v22 release `5282cfde546ea2b5` was built from `dad7aa8`, including
`c6e5538` and a focused opening Help checklist. The first exact-stage VM attempt
stopped because concurrent serial messages split `deskterm: exited failure=0`;
a separate compositor child-exit record showed status zero. That failed attempt
was preserved. The unchanged candidate then passed the complete two-boot desktop,
persistence, unmount and audio rehearsal before installation.

The user reports that the Terminal character issue appears resolved and
explicitly validated Ctrl+C, Ctrl+V and Ctrl+A in Text. Matching physical session
`session-00065e37be14b48c-00` was collected after return: four hash-verified logs,
saved exit snapshot, clean desktop process exit and zero collector attention
flags. No new screenshots were present. The offline read-only exFAT check was
clean (164 directories, 822 files). The one-shot boot selection was consumed;
current is `5282cfde546ea2b5` and accepted previous remains `b427b78f21644bd2`.

Terminal backspace redraw and Text copy/paste/select-all are physically accepted
for this machine. This report does not separately establish secondary press/tap
behavior, cancellation fault paths or power telemetry. No further reboot is
armed.
