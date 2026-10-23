# MacBookAir6,1 read-only temperature and fan trial

Native readings are pending. This profile uses the existing cached CPU thermal
and bounded SMC services. It does not control fan speed, charging, thermal policy
or power limits, and does not grant battery/EC/AML access.

## Evidence and admission

The actual i5-4260U reports GenuineIntel signature `00040651`, CPUID.1 ECX
`7ffafbbf`, EDX `bfebfbff`, and CPUID.6 EAX `77`: MSR, core DTS and package
thermal support, without the hypervisor bit. Read-only Ubuntu MSR capture gave
`1a2=640000`, `19c=88240800`, `1b1=88240800`: 100 C target with zero TCC offset
and decoded core/package 64 C. Nearby Linux coretemp samples were 62–63 C;
these sequential observations are not an atomic comparison or calibration.
The existing model 3d/46 gates remain; the new signature is exactly `40651`.
Hypervisors, missing features, extended-family mismatch and other model-45
steppings remain refused. No MSRs are written.

The current DSDT is byte-identical to this unit's October 22 capture: 31777 bytes,
FNV-1a `af9a47bc352dc6bf`. With GPU `8086:0a26` and PCH `8086:9c43`, that exact
identity selects one fan. Its LPCB.SMC APP0001 declares Decode16 I/O `300..31f`,
MMIO `fef00000..fef0ffff` and IRQ6. Only command/status `304` and data `300`
ports are used. The latter two resource types are not mapped or owned. Linux
applesmc reports one Exhaust fan, 1202 RPM at capture, minimum 1200, maximum
6500 and manual mode zero. Linux keeps ownership during capture; no raw SMC
transactions were issued from Ubuntu.

On SrvrOS startup the existing worker validates FNum metadata (`ui8 `, readable,
length 1), requires count 1, and validates F0Ac (`fpe2`, readable, length 2).
Only metadata/read commands are allowed. F1Ac is never requested on this profile.
Unexpected metadata/count or transport failure quarantines the provider and
invalidates the cached reading. Fan control/mode keys are never written.
SMC read requests themselves require command/argument port writes; read-only
means no sensor/control-key writes. Firmware retains its fan policy.

## Build and rehearsal

```sh
tools/host-env.sh make -j2 all usb-image test-core test-desktop
python3 tools/test_hardware_boot.py
build/tests/laptop-resources-host build/hardware/macbookair6-1-2026-10-22/acpi air61
python3 tools/hardware_boot.py stage --desktop-volume 6ADA-85BB --desktop-basic --desktop-brightness --desktop-audio --desktop-sensors
tools/host-env.sh python3 tools/hardware_boot_smoke.py --desktop-check --audio-check --workspace-table gpt
```

The new v59 recipe adds only CPU/SMC reads to accepted v57 desktop/brightness/
audio. v58 belongs to the separate shared device checklist. Existing recipes
remain immutable. Generic QEMU must report unavailable thermal/SMC providers;
its two-session desktop/audio rehearsal cannot prove native sensor reads.
Host tests cover CPU feature/signature refusals and the captured decode, exact
firmware positive/mutation refusals, one-fan metadata/count/sampling, cached
queries, transport timeouts and invalidation, and existing Air/Pro profiles.

## Attended native checklist

1. Open System and select Sensors. Allow several seconds for initialization.
   CPU core/package temperatures and one fan RPM should appear with sample ages
   normally below three seconds. The core reading is the boot CPU, not all cores.
   Note unavailable/stale/error states; take a screenshot with Command-Shift-3.
2. Leave Sensors visible for about two minutes while moving windows, typing in
   Text and saving a small document. Check that values/ages refresh and input
   stays responsive. Fan RPM may remain near its minimum; a change is not required.
3. In Terminal play `playwav /fat/share/examples/stereo.wav`; confirm sound works
   while readings continue. Try Sensors Pause, wait, then resume/Refresh. A paused
   display is deliberate; after resuming the samples should become fresh again.
4. Close/reopen System, check fresh readings, and take another screenshot. Report
   approximate temperature/RPM ranges and any stale values or input/audio issue.
5. Exit the desktop normally, wait for USB unmount and type `reboot` at `srv>`.

Do not force thermal load or change fan settings. This first trial checks live
readings and coexistence, not accuracy calibration, cooling performance, thermal
limits or battery support. Return collection must show matching release, fresh
samples, no SMC faults, clean desktop exit and preserved USB files/allocation.
Ubuntu remains default and accepted audio/console payloads remain recoverable.
