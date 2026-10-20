# MacBookAir7,2 / A1466

Updated: 2026-10-19. State: **active reference machine; partial device acceptance**.

## Identity and ownership

Development host: Ubuntu on the 13-inch Air, Broadwell-U, Intel HD Graphics 6000
`8086:1626`, xHCI `8086:9cb1`, Apple topcase `05ac:0291`, HDA `8086:9ca0` /
CS4208 `1013:4208` subsystem `106b:7e00`. Firmware, descriptors and routing evidence
are in the linked device guides. The shared integration baseline is now `main`,
including the completed `codex/linux-development-baseline` work. Subsequent
device work uses separate branches based on `main`.

## Active work and shared dependencies

- Owner: local Air7,2 device agent; branch `device/macbookair7-2/reliability-input-power`.
- Objective: read-only desktop power/frequency baseline, then bounded native
  battery/AC runtime groundwork; CPU policy changes and suspend/resume follow
  measurement and separate acceptance.
- Shared scope: power telemetry/collector first; subsequent ACPI/EC runtime,
  worker/IRQ ownership and battery interfaces require explicit coordination.
  No new native EC/SCI enablement is part of the power capture.
- Integration baseline: `main` merge `b6075ef`; PR #4 merged after review and
  both exact-head CI runs passed at `869cbc7`. Merge tree matches the tested head.
- Power checkpoint: v26 `ed93e03b2585dcaf` accepted for bounded read-only
  telemetry, 6.458 W package average / 2,083 MHz CPU0 active over 52.999 seconds.
- Current implementation block: complete the connected EC/IGPU access audit,
  then build an opt-in native battery/AC diagnostic through a separate runtime
  adapter, serialized EC/SBS worker, owned SCI/GPE path, bounded lifecycle and
  durable test reporting. Scope includes runtime/transport, build/staging tests
  and guides. Keep normal desktop startup and accepted recovery unchanged.
  Dependencies: chipset/handoff `a51c4a5`, PM1 notification `2d29126`; integration
  baseline `b6075ef`. Pro11,2 PR #5 has advanced to `203e2d1` with exact Pro
  brightness and shared staging changes. Its profiles occupy v27–v30; this
  battery capture reserves **v31**. Native ACPI code does not overlap. Shared
  staging/test hunks need integration reconciliation without changing existing
  release recipes; neither branch is silently merged into the other.
- Native battery candidate: connected captured initialization now passes the
  offline model, including ECSS, GNVS lid/AC and IGPU CLID effects. The separate
  Air-only runtime performs five bounded SBS/AC samples, explicit connection,
  owned SCI/GPE handling and clean retirement; ordinary desktop retains a
  refusal stub. The same lifecycle passes two Q35 lifetimes and six injected
  faults (denied region, EC stall, storm, setup/query/disconnect deadlines).
  The native image refuses Q35 before hardware access. Host memory policy,
  transport/SBS/event/owner tests and collector/staging tests pass. Physical
  battery/AC values, actual EC SCI delivery and firmware handoff remain pending.
  See [native runtime gate](../native-ec-runtime.md#explicit-native-battery-diagnostic).
- PM1 notification checkpoint: all 65,536 starting words and each single-bit
  command mutation pass host/sanitizer checks. The Q35 release case performs
  exactly one actual emulated PM1 write, refuses permission replay, preserves
  readable control bits and cleans up through two lifetimes. Injected SLP_EN is
  rejected before any write and halts with state retained. Firmware Pending is
  modeled; this does not accept native firmware reception. Generic PM1 writes
  remain denied. Connected Air EC `_REG` additionally writes the IGPU OpRegion's
  CLID field; its mapping needs a captured layout/per-access audit before binding.
- Chipset/handoff checkpoint: read-only Linux capture confirms missing PCI
  IDs/configuration and RCBA/HPTC; all nine live firmware hashes match. Complete
  offline initialization now passes with an exact read-only DWORD model and
  captured PCI snapshots (zero errors/denials/leaks, no PCI writes on this path).
  Q35 modeled firmware-to-OS lock notification and an owner-side operation
  deadline pass. This accepts neither native SCI nor OS-to-firmware handoff;
  connected EC `_REG`, native access policy and controller ownership remain.
- Earlier Global Lock checkpoint: Q35 full-uACPI tests pass two acquire/release
  lifetimes, firmware-held lock containment and denied PM1 handoff detection.
  Owner drain now precedes interpreter reset; a stuck worker retains heap,
  Owned/Pending bits and the masked SCI callback. Exact fixture lock-word reads
  are allowed; the later PM1 notification fixture above adds a scoped release.
  Native contention/handoff, operation ownership and live mappings are pending.
- Earlier full initialization refusal checkpoint (superseded by chipset replay): offline `--initialize` refuses errors,
  warnings and denied accesses even when uACPI returns success; skips follow-up
  queries on refusal. Twenty-five synthetic tests cover opt-in execution,
  ignored errors, malformed status and prior load faults. Captured Air replay
  with two private GPIO byte models failed on HPET chipset memory and
  missing PCI snapshots (four AML errors / 11 denials / zero remaining
  allocations). GN46 maps to input-sensing disable; native access remains gated.
  Full-runtime FACS contention and PM1 Global Lock release need their own tests.
- Pipe-restart checkpoint (`3e70395`): the sending helper blocks its process-wide
  test signal so its own yields cannot consume it. The probe verifies the handler
  ran on the reader; 256 rounds with varied yields and the full 481-command CLI
  suite pass, with clean exFAT. This corrects the test protocol, not the broader
  POSIX signal implementation. The previously failing old-head CI passed on
  retry; the new repeated regression is retained in CI.
- Memory audit checkpoint: private memory/PCI access traces and budgets pass
  21 namespace tests normally and under ASan/UBSan. Captured initialization's eight writes only change OSYS;
  one PCI read and the constrained GPIO access are recorded. Separate `_SB._INI`
  correctly refuses an additional GPIO read at `0xa74` for `GN46`; its live
  meaning/ownership remain unaccepted. Input captures remain unchanged. See the
  [runtime guide](../native-ec-runtime.md) for exact scope and counts.
- SCI ownership checkpoint (`fbdf014`): the reusable owner now prepares a masked
  route, registers its callback, enables delivery, and retires/restores the
  route with bounded rollback. Host fault tests and ASan/UBSan pass, including
  immediate delivery, partial writes and poisoned-state retention. The separate
  Q35 runtime passes three SCI lifetimes and failed-install rollback after
  actual route preparation; early EC load passes two lifetimes with queries,
  SBS samples and disconnect cleanup. Blocked-worker and interpreter self-reset
  cases halt visibly with live state retained. These are VM/host results only;
  native source ownership and live mappings remain pending.
- Query containment checkpoint: a drain now retains its attempt/time budget
  across worker batches and additional notifications. Host/sanitizer cases pass
  for finite/repeated drains, queued and AML time, clock regression and sticky
  faults. The early-load VM regression passes; continuous and zero-reply storms
  each halt at 128 attempts, and a three-second AML query fails its two-second
  cooperative budget after returning. All retain live state with interrupts off.
  CI now runs the separate SCI/early-EC and query-fault VMs sequentially and saves
  their serial evidence. Native GPE masking/rearm and a worker that cannot return
  still need the external ownership/lifecycle path; no physical result is claimed.
- Event-register checkpoint (`843b66c`): complete supported FADT event layout
  decoding and exact-width/direction IO policy pass host tests and ASan/UBSan,
  including the captured Air FADT. Q35 now restricts fixed enables to its Global
  Lock/power button and enables no GPEs. SCI lifecycle and early EC/SBS/teardown
  regressions pass under the policy. An injected early write denial is refused
  despite upstream API success, then cleans up with zero heap before a normal
  start. Full startup clears all GPE status bytes: native ownership must cover
  that domain, and cleared pending events cannot be rolled back from a snapshot.
  Live native register/NVS/FACS/PCI/GPIO permissions remain unaccepted.
- Dependencies: early worker/handler lifetime fixture merged in PR #3; actual
  captured Air firmware, legacy/extended address consistency, namespace resource
  agreement, `_OSI` policy and SCI/GPE ownership must all be resolved before use.
- Resource checkpoint: [offline EC contract audit](../native-ec-runtime.md)
  matches captured ECDT/FADT to the loaded EC Device/HID/UID/GPE/CRS, with absent
  `_GLK`, zero errors/denials/leaks and private snapshots only. Generic table
  and namespace refusal tests pass. A separate PCI0 `_INI` replay refuses the
  GPIO mapping at `0x800`. The follow-up private byte replay completes PCI0
  `_INI` with one read/write at `0xb90`, setting only bit 3. The same replay
  passes a set-only bit-3 rule and rejects a wrong mask; native GPIO effects
  and ownership still need acceptance. Eighteen namespace tests and the pure
  per-access rule's exhaustive byte transitions pass, including ASan/UBSan.
  This is not controller ownership acceptance. The production build passes and
  its PT_LOAD contents match installed `ed93e03b2585dcaf` exactly.
- First native battery trial: v31 `cc486bf6b6581626`, source `8dc2408`,
  refused platform/resource preflight before runtime initialization. Session
  `session-00065e3d0d7c3f18-00` was copied and hash-verified after return;
  exit logs were saved and the USB unmounted before the persistent halt.
  No battery samples or native EC/SCI acceptance. The screen's `saved=NO`
  combined diagnostic failure with log saving and did not describe the saved files.
- Granular native trial `339211cceb5d894d` (source `e4c8d7e`) refused
  GPE enable byte 12 at `0x189c`, value `0x40` (GPE `0x66`), before EC/runtime
  ownership. Session `session-00065e3d35a068b6-00` verified; logs saved, USB
  unmounted, persistent halt photographed. One-shot consumed on return.
- Current bounded follow-up: preserve the captured graphics/TCO SCI event
  (`_L66`) while the separate battery diagnostic owns EC GPE `0x4e`. Add an
  explicit mask/readback/restore lease and protect foreign W1C status, with
  host fault tests and a VM fixture. Do not dispatch graphics AML or broaden
  register permissions. Unknown enabled events remain refused. Native handoff
  and battery readings remain unaccepted; normal desktop unchanged.
- Handoff implementation: the known graphics event is leased only from legacy
  mode, its status excluded from uACPI clears, and its original enable restored
  after clean runtime retirement. Host tests and ASan/UBSan pass; two VM
  lifetimes retain a pending modeled event. Mask/restore failure and unexpected
  reenable cases halt; the existing battery fault regressions pass.
  Candidate `c0f2c3af0dbacc66` (source `8dc82e7`) passed exact GRUB/UEFI
  Q35 refusal, saved log-byte verification and clean exFAT. Native acceptance
  remains pending; the local release run record owns installation/reboot state.
- Integration remains `b6075ef`; Pro PR #5 is at `827d035` with native cursor
  timing evidence. Battery event lifecycle is this Air branch's scope; shared
  startup/staging hunks still require reconciliation during integration.
- Recovery: current is `339211cceb5d894d`; previous `b427b78f21644bd2` remains
  installed. Accepted v26 and interactive payloads have verified copies outside
  build. Ubuntu returned with one-shot consumed; no reboot is armed.

## Accepted scope and open items

| Capability | State | Evidence / limit |
| --- | --- | --- |
| Desktop boot, rendering, keyboard, trackpad, USB documents | Physically exercised | User rounds and durable workspace history in the development journal |
| Terminal backspace redraw | User validated | `5282cfde546ea2b5`, fix `c6e5538`; reported stray characters no longer appear |
| Text Ctrl+C / Ctrl+V / Ctrl+A | User validated | `5282cfde546ea2b5`; copy, paste and select-all explicitly checked |
| Held-click second-finger steering | User accepted | `d61e60a82a295d84`; counters do not independently prove every handoff |
| Brightness and audible volume adjustment | User accepted | `b427b78f21644bd2`; requested brightness 16/83 applied, three tone tests completed |
| Temperature and fan sampling | Live output confirmed | Fresh screenshot readings 83 C / 1202 RPM; logs reached 85 C, no SMC errors; calibration/power behavior not accepted |
| Two-finger physical right-click | Implemented; physical acceptance pending | Press role latches from initial contact count; one-finger press plus later steering finger remains primary |
| Context menu from trackpad taps | Physical acceptance open | Exit counts 5 primary and 1 secondary tap; user reported no visible right-click result |
| System window stability | No recurrence in latest physical round | `fed370ffbe33d554`: System and Help exited cleanly, no collector attention flags; earlier timeout cause remains unproven |
| PCM files / headphones | VM-tested / physical confirmation incomplete | Volume report does not separately accept these routes/workflows |
| Package power / CPU0 active frequency | Physical diagnostic accepted | `ed93e03b2585dcaf`; bounded Text idle capture, no CPU controls or whole-laptop power |
| Battery/AC and suspend/resume | Not implemented as live services | ACPI/EC foundations exist; runtime ownership and tests remain |
| Optional isolated AML probe | Native completion accepted | `4e5f438c4069752b`; resource queries, zero errors/denials/leaks; v22 still omits this diagnostic |
| USB Ethernet | Implemented CDC ECM, VM-tested | No actual adapter accepted |

## Latest interactive physical record

Release `5282cfde546ea2b5`, v22, source `dad7aa8`, contains the Terminal cursor
correction `c6e5538`. The user reports that the character artifact appears
resolved and explicitly validated Ctrl+C, Ctrl+V and Ctrl+A in Text. Those two
workflows are now physically accepted on this Air.

Matching session `session-00065e37be14b48c-00` has four verified logs, a saved
exit snapshot, clean desktop process exit and no collector attention flags.
There were no new screenshots in this round. Offline read-only exFAT checking
was clean (164 directories, 822 files). Evidence and the run record are local
under `build/native-desktop-trials/5282cfde546ea2b5/`.

At that interactive checkpoint installed was `5282cfde546ea2b5`; previous remained accepted recovery
`b427b78f21644bd2`, and no one-shot boot is armed. Secondary press/tap cases were
not individually confirmed in the report; power telemetry was not enabled.

## Earlier October 19 interactive round

Release `fed370ffbe33d554`, v22, source `c6e50ec` (same tree as merged baseline
`e2d172b`), was exercised on October 19. The user reported generally good
behavior, with a new Terminal backspace display defect. Session
`session-00065e37230cc95f-00` has four verified logs, a clean desktop process exit
and no collector attention flags. Two saved screenshots are retained locally in
`build/native-desktop-trials/fed370ffbe33d554/physical-after/`.

System and Help remained present in the screenshots and exited without failure.
PCM playback completed 1,152,000 bytes with zero reported underruns; the user did
not separately confirm channel separation or headphones. The Sensors screenshot
shows 67 C core/package and 1,209 RPM with fresh sample ages. The offline exFAT
check was clean (163 directories, 818 files). General success does not establish
every gesture case: the final two-finger tap counter was zero, so specific
secondary-tap acceptance stays open.

At that checkpoint installed was `fed370ffbe33d554`; previous remained accepted recovery
`b427b78f21644bd2`. The one-shot boot selection was consumed and no next boot is
armed. The Terminal screenshot's stray slash was reproduced in the host editor
and actual desktop VM: erasing padding left the output cursor beyond the edited
line. The source correction restores that cursor; the subsequent physical
retest above accepts that correction. See the [journal](../desktop-progress-2026-10-19.md).

## Earlier interactive physical record

Release `b427b78f21644bd2`, v22, built from source `5b1ed05`.
Session `session-00065e281812e807-00` has saved logs and clean desktop exit.
Local evidence is in `build/native-desktop-trials/b427b78f21644bd2/physical-after/`.
The detailed [journal](../desktop-progress-2026-10-18.md) preserves failures,
partial acceptance and the previous implementation sequence.

At this checkpoint current is `b427b78f21644bd2`, previous is `f05dfcc01c28dc85`,
and the independent held-drag backup is under
`~/.local/share/srvros/known-good/d61e60a82a295d84`. Re-read actual installed state
before any action; these are historical identifiers, not values for other Macs.
The local trial record marks this run returned/reviewed, not armed for reboot.

## Next actions

Current shared-code work is on `device/macbookair7-2/reliability-input-power`
from `main` at `3b0b43a`: physical click latching, compositor delivery deadlines,
idle/sleep scheduling and console polling. The [October 19 journal](../desktop-progress-2026-10-19.md)
records host/VM evidence. Native automated batch `54a944ae1f7849ac`
(source `686edd8`) passed all five filesystem/shell rounds with retained files
verified and a clean offline exFAT check. The following bounded console-idle capture `1fe693c9889f131f`
(source `b3f0e31`) saved 60 seconds of samples: 94.55% idle, package 80→75 C,
1,206 RPM final fan, no SMC errors. It returned automatically with a clean
offline exFAT check. Subsequent isolated AML capture `4e5f438c4069752b` (source `17c6ed3`)
completed cleanly in 5.13 seconds and returned with clean exFAT. Unattended
desktop capture `06c646455f08555e` (source `e6fd681`) subsequently measured
60 seconds with Text open: 94.15% CPU0 idle samples, one frame presentation and
76,384 framebuffer bytes during the measurement. All four logs and the 1440×900
BMP were verified, with clean automatic return and offline exFAT. At that earlier checkpoint the current boot
payload was the desktop-idle diagnostic, with accepted desktop
`b427b78f21644bd2` retained as previous. The newer interactive record above
supersedes that installed-state snapshot; power consumption still needs
physical acceptance. Timer samples measure CPU0 scheduler occupancy, not energy.
The generalized Linux collector completed with zero partial reads on this host.
Additional IRQ, semaphore and deferred-worker foundations now pass host/VM tests.
A separate full-uACPI Q35 kernel passes real SCI, contended AML mutex/sleep and
teardown tests, including worker-owned EC disconnect I/O and halting with
state retained on a poisoned controller or a disconnect deadline. It is not the
installed payload or a native EC/battery provider. Its immutable `6b7c2bc8`
artifact passed a verified 30-minute, 37-round runtime/EC soak. Later explicit
synthetic namespace initialization checks worker ownership and refuses readiness
when firmware methods log denied accesses even if the API returns success.

1. Physically accept two-finger secondary click while preserving established primary drag; host/transport tests precede this check.
2. Follow up the System timeout after the delivery fix; the native cause is not yet proven.
3. Extend the accepted console and unattended desktop idle measurements to interactive activity and actual power/frequency evidence, retaining USB/input responsiveness.
4. Battery/AC runtime, headphone route acceptance and actual USB networking.
5. Shared inventory/profile/scaling work for the other model agents, coordinated
   through the [expansion plan](../apple-hardware-expansion-plan.md).

Shared desktop preparation now includes explicit integer 2× scaling. UEFI VM
checks passed at 2880×1800/2× and the Air's 1440×900/1×, including exact capture
pixels and input/window geometry. The installed Air capture exercised the default 1× path; the additional Pro
models still require their own framebuffer and input acceptance.


Read-only [package-energy and CPU0 active-frequency telemetry](../power-telemetry.md)
has host arithmetic/service tests, VM refusal evidence and the accepted
October 19 v26 native capture. It remains opt-in. The Linux MSR capture confirms register availability
on this host but is not an SrvrOS power result. No CPU power controls have changed.

Trackpad transport/palm faults now cancel a held interaction rather than deliver
an ordinary release. The compositor also preserves queued typing on pointer
cancellation. Host fault tests cover both paths; native acceptance is pending,
and this is not claimed as the cause of earlier unexplained window closures.

The generalized Linux inventory now includes `cpu-policy.json`. The October 19
capture had no partial reads: four online logical CPUs, `intel_idle`/`menu`, and
four `intel_cpufreq`/`schedutil` policies with 500–3,200 MHz limits. CPU0 listed
POLL through C10 as enabled. These are Linux policy observations, not SrvrOS
energy or deeper-idle acceptance. The [power guide](../power-telemetry.md)
documents units, non-atomic capture and missing-field semantics.

## Latest automated power record

Release `ed93e03b2585dcaf`, source `0f2038a`, session
`session-00065e38449c2146-00` completed its 60-second Text idle capture and returned
to Ubuntu. Valid power checkpoints cover 52.999 seconds: 6.458 W average package
power and 2,083 MHz CPU0 active frequency, with 94.10% scheduler-idle samples over
the full desktop interval. See [power telemetry](../power-telemetry.md#accepted-air72-desktop-baseline)
for units, conditions and limits. Four logs and the screenshot verify; no collector
attention or rejected power samples; offline exFAT clean (165 directories,
827 files). Evidence is backed up under `~/.local/share/srvros/evidence/ed93e03b2585dcaf`.

Current installed is this automated diagnostic; previous remains accepted
`b427b78f21644bd2`. The one-shot is consumed. Interactive `5282cfde546ea2b5`
is independently backed up. No additional physical trial is armed.
