# MacBook Air profile parity

Reviewed October 28, 2026 against main `5a94d755` and its merged Air6,1 work.
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
| Audio/disk/app/screenshot stress | v74 bounded 30-minute native run, 703 app cycles; v77 attended variant | New v80 automatic and v81 attended profiles reuse the same workload, deadlines and ownership. Air7,2 has not inherited the 30-minute result. |
| Desktop power phases | v78 idle/apps/recovery; worker-wakeup receipts and native measurements | New v82 uses the Air7,2 backend with the same phases, read-only power and normal return. No inferred energy saving. |
| Idle residency and capture prerequisites | Ten counters; HLT entry, E2/APIC/misc and topology audit | Captured Broadwell support adds seven counters and the shared audit. C8/C9/C10 remain unavailable, not valid zeros. Native diagnostic remains a separate gate. |
| Explicit MWAIT experiments | Bounded shallow and C3-hint execution accepted; zero observed C3 residency | Broadwell MWAIT remains refused until its own prerequisites/selector review and bounded trial. Read-only counter admission does not enable instructions or copy Haswell policy. Ordinary idle remains HLT. |
| Secondary CPU ownership | Investigation/design only; AP rendezvous is the next proposed milestone | No implemented AP/SMP capability to copy. Preserve the Air6,1 scope; do not add a Limine MP request to ordinary boot. |
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
confirms the seven reads and unchanged E2/APIC/misc snapshots. These are admission
prerequisites, not native SrvrOS acceptance or an energy comparison.

The existing vendor/feature/hypervisor/reference-clock gates apply first, then
invariant TSC and the captured model choose mask 127 (Broadwell) or 1023
(Haswell-ULT). Only selected MSRs are read. The sampler retains separate core and
package scopes and existing reset/skew/overflow rejection. Collection requires a
consistent complete model mask and omits unsupported counter names. The explicit
CPU-load capture also records HLT entry and CPU0 topology/configuration; it does
not observe remote CPUs or establish time asleep. Broadwell MWAIT remains refused.
