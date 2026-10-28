# MacBook Air profile parity

Reviewed October 28, 2026 against main `5a94d755`, then integrated PR232
(`80b0a8aa`) while retaining the distinct AP diagnostic gate.
The [Air7,2 record](machines/macbookair7-2.md) and
[Air6,1 record](machines/macbookair6-1.md) own independent native acceptance.
This compares source support and test coverage; firmware, USB volume identities,
CPU selectors and physical results are not interchangeable.

| Area | Air6,1 coverage | Air7,2 parity and remaining check |
| --- | --- | --- |
| Desktop/input/widgets | Independent native desktop, preferences, drag/scroll and bounded workload evidence | Shared desktop/runtime is already integrated; Air7,2 additionally has bounded secondary-click and recent navigation/Close acceptance. No missing widget implementation identified. |
| Brightness, audio, CPU/SMC | Exact Haswell, CS4208 and one-fan profiles; bounded native controls and playback | Separate captured Broadwell, codec and one-fan profiles already exist and have native evidence. Retain those gates. |
| Battery/AC | Separate EC/SMBus ownership, indicator and combined-session acceptance | Existing Air7,2 backend/indicator remains in place; shared worker and supervisor event waits are integrated. Newer polling reductions need matching native measurements. |
| Combined reopen | v72 automated two-session test and native acceptance | New v79 selects the same mechanism with the Air7,2 kernel; local and native evidence must name this profile. |
| Audio/disk/app/screenshot stress | v74 bounded 30-minute native run, 703 app cycles; v77 attended variant | New v80 automatic and v81 attended profiles reuse the same workload, deadlines and ownership. Air7,2 has not inherited the 30-minute result; three v80 native runs failed early lifecycle deadlines and remain unaccepted. |
| Desktop power phases | v78 idle/apps/recovery; worker-wakeup receipts and native measurements | v82 release `775c5051149c0e38` accepts three native phases, 13 app cycles, bounded loop/ready/close and clean retirement. No inferred energy saving. |
| Idle residency and capture prerequisites | Ten counters; HLT entry, E2/APIC/misc and topology audit | Captured Broadwell support adds seven counters and the shared audit. C8/C9/C10 remain unavailable, not valid zeros. Native CPU0 diagnostic accepted on release `71049508c4a1fbd0`: complete seven-counter capture, unchanged configuration and zero observed deep-idle deltas. |
| Explicit MWAIT experiments | Bounded shallow/C3-hint execution accepted; PR237 additionally accepts coordinated core C3 with valid-zero package C3 | Broadwell MWAIT/coordinated C3 remain refused until their own prerequisites/selector review and bounded trials. Read-only counter admission does not enable instructions or copy Haswell policy. Ordinary idle remains HLT. |
| Secondary CPU ownership | PR232 adds a separate bounded AP rendezvous payload; PR233 records native Air6,1 acceptance | Shared diagnostic is integrated, with its native Haswell-only admission intact. Broadwell needs independent identity/MONITOR/APIC prerequisites and a separate native trial after CPU0 capture. Ordinary boot has no MP request; general SMP scheduling remains absent. |
| Network/other devices | No accepted Ethernet/Wi-Fi on that unit | Air7,2 already has matched USB ECM acceptance. No parity gap justifies broadening adapter or firmware gates. Suspend/resume, internal Wi-Fi, Bluetooth and camera remain separate. |

## Explicit desktop profiles

The existing Air6,1 v72/v74/v77/v78 configurations remain byte-for-byte unchanged.
Use the existing flags with the Air7,2 kernel (omit `--battery-air61`):

```sh
python3 tools/hardware_boot.py stage --desktop-volume "$SRVROS_USB_UUID" \
  --desktop-battery --desktop-services --desktop-reopen-test
# v79: automatic two-session reopen

python3 tools/hardware_boot.py stage --desktop-volume "$SRVROS_USB_UUID" \
  --desktop-battery --desktop-services --desktop-audio-io-test
# v80: automatic 30-minute audio/disk/app workload

python3 tools/hardware_boot.py stage --desktop-volume "$SRVROS_USB_UUID" \
  --desktop-battery --desktop-services --desktop-audio-io-attended
# v81: attended workload with in-guest instructions, manual normal exit

python3 tools/hardware_boot.py stage --desktop-volume "$SRVROS_USB_UUID" \
  --desktop-battery --desktop-services --desktop-power-test
# v82: automatic idle/app/recovery power phases
```

Choose one test per stage; these recipes omit networking for comparable bounded
workloads. They enable no optional AML probe or MWAIT policy. Kernel selection
remains explicit: `build/acpi-battery/srvros.elf` for Air7,2,
`build/acpi-air61-battery/srvros.elf` only with `--battery-air61`.

Rehearse exact stages with `battery_home_desktop_smoke.py --boot-session
--services --automatic --case profile-refusal --stage <release>` for v79,
`audio_io_stress_smoke.py --stage <release>` for v80,
`audio_io_attended_smoke.py --stage <release>` for v81, and
`desktop_power_smoke.py --stage <release>` for v82, under `tools/host-env.sh`.
These native kernels must refuse Q35 ownership; successful refusal/save/return is
not the positive test. Run the synthetic positive fixtures too: omit `--stage`,
add `--air72` to the power/audio harnesses, or omit `--air61` from the automatic
Home harness. Keep legacy Air6,1 fixtures in the regression set.

Follow the [Ubuntu workflow](ubuntu-device-agent-setup.md) before physical use:
resolve the local USB UUID, verify recovery and prior files, retain an on-disk run
record, install only the rehearsed stage and use a one-shot boot. Automatic tests
show progress and persist results before Ubuntu return; fatal mounted ownership
or failed unmount may still halt for containment. An attended v81 run needs
explicitly announced human input and the complete in-guest checklist.

## Broadwell read-only counter contract

Intel [SDM Volume 4, section 2.15](https://cdrdv2-public.intel.com/929363/335592-093-sdm-vol-4.pdf)
lists the inherited tables for 06_3D. Linux
[v6.12 cstate profiles](https://github.com/torvalds/linux/blob/v6.12/arch/x86/events/intel/cstate.c)
separately maps Broadwell to the seven-counter set, while Haswell-ULT includes
C8–C10. The local Air7,2 signature is `0x306d4`; read-only Ubuntu CPU0 capture
confirms the seven reads and unchanged E2/APIC/misc snapshots. The matching native CPU0 diagnostic now confirms bounded accessibility and unchanged
configuration as recorded below; neither capture establishes energy savings.

The existing vendor/feature/hypervisor/reference-clock gates apply first, then
invariant TSC and the captured model choose mask 127 (Broadwell) or 1023
(Haswell-ULT). Only selected MSRs are read. The sampler retains separate core and
package scopes and existing reset/skew/overflow rejection. Collection requires a
consistent complete model mask and omits unsupported counter names. The explicit
CPU-load capture also records HLT entry and CPU0 topology/configuration; it does
not observe remote CPUs or establish time asleep. Broadwell MWAIT remains refused.

## Workload regression found during parity validation

The new Air7,2 fixture initially exceeded the unchanged 500 ms desktop-loop bound
under TCG (570 ms) and KVM (527 ms), despite successful PCM/disk verification and
clean retirement. A traced repeat measured 560 ms, including 313 ms READY time,
no blocked wait and no I/O-gate exclusions. These failures remain retained.

The compositor was synchronously printing each launch receipt. It now leaves that
receipt to the existing lifecycle worker, retaining the launch record until the
write finishes. A host test deliberately stalls that writer and verifies that the
UI side remains runnable and the record remains owned. Follow-up Air7,2 fixtures
pass with worst loops 122 ms (TCG, 26 cycles) and 140 ms (KVM, 23 cycles), zero
underruns, exact file checks and clean retirement. The workload, loop and lifecycle
limits are unchanged. This bounds the observed improvement; native timing and the
full 30-minute Air7,2 run still require matching hardware evidence.

Audio workload diagnostics also record scheduler attribution for lifecycle
intervals above 100 ms. These trace calls are confined to workload diagnostics;
ordinary desktop operation does not enable them. `--accel tcg|kvm` explicitly
selects the audio VM accelerator, with the original TCG default preserved.

## Air7,2 native CPU0 checkpoint

Release `71049508c4a1fbd0`, source `5bab654b`, session
`session-00065ee8fc3ded29-00` completed the ordered 30-second idle/load/recovery
phases and returned to Ubuntu automatically. Covered package averages were
6.782/8.677/6.825 W. All seven supported residency deltas were zero; unsupported
C8–C10 remain omitted. HLT entry/return pairs and unchanged E2/APIC/misc snapshots
validate the diagnostic without demonstrating deep idle. CPUID topology describes
two cores/four addressable logical CPUs; it does not observe their running state.
No AP startup, MWAIT or policy write occurred. Prior files, offline filesystem and
allocation checks, consumed one-shot and independent recovery all pass. The later native desktop checkpoint below covers power phases; full 30-minute
endurance remains pending.

## Air7,2 native desktop power checkpoint

Automatic v82 release `775c5051149c0e38`, source `0cbbb275`, session
`session-00065ee927f533d1-00` passes all three 60-second phases and native
battery retirement. Thirteen app cycles complete with 116 ms maximum loop,
1.471 s ready time and 103 ms close time. Covered package averages are
6.827/6.722/6.666 W; these do not demonstrate an energy saving. Checkpoints retain
worker transition/wakeup counts and native temperatures. All prior files and
both offline integrity checks pass; automatic Ubuntu return, consumed one-shot
and independent recovery are verified. Full 30-minute audio/disk/app/screenshot
endurance is the next automatic physical check.

## Endurance failure and I/O admission follow-up

The first v80 native release `0d1ffb15c35f639a` did not complete 30 minutes.
At cycle 6, Text reached its `main-return` receipt but the compositor did not reap
it before the unchanged 10-second lifecycle deadline. Screenshot saving overlapped;
its completion also failed. Maximum loop was 250 ms, PCM underruns were zero,
and native battery retirement was clean. All prior files and offline integrity
checks passed. Preserve the failure rather than treating automatic return as a pass.

A separate deterministic scheduler regression exposes starvation in quiescent I/O
handoff: restarting after the writer lets a recurring nearby waiter win all 32
turns over another contender. The proposed correction rotates after the last
admitted waiter, including ordinary scheduling admissions. Both waiters receive
16 turns in the host fixture; storage ownership, eligibility, workload and deadlines
remain unchanged. Native causation/acceptance still require a matching retry.

The fairness candidate passes the normal Air7,2 KVM workload (23 app cycles,
zero underruns). The throttled 100-write-IOPS fixture still fails loop and close
bounds. Treat fairness as a separate proven correction, not a complete resolution
of heavy-I/O latency. Failed diagnostic receipts now include a bounded read-only
process-state snapshot to distinguish an active process from an inactive record with retirement/reaping pending; thresholds
and early failure behavior remain unchanged.

PR238 moves terminal-child blocking reap and screenshot preparation to the
existing lifecycle worker, retaining owned PIDs, unpublished jobs and immutable
canvas snapshots through shutdown. Its third native attempt, release
`5f4ed45c3634e22a` / session `session-00065eea4580440f-00`, completes nine app
cycles with a 101753 us maximum UI loop, but fails the unchanged total lifecycle
deadline during Text document release (6067465 us wall time). Screenshot
completion fails; no PCM underruns are observed and native retirement is clean.
All prior files and offline filesystem/allocation checks pass. Endurance remains
unaccepted. Sparse current-thread scheduler attribution is the next diagnostic;
wall time does not justify changing kernel memory/I/O gates or pacing the load.
