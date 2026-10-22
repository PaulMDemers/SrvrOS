# MacBookAir6,1 — local bring-up

Updated: 2026-10-22. State: **Linux inventory captured; host build/core and baseline VM checks passed; physical tests pending**.

## Identity and ownership

- Actual DMI model: MacBookAir6,1; firmware `122.0.0.0.0`.
  Marketing year and storage replacement history are not independently verified.
- Branch: `device/macbookair6-1/bringup`; integration base
  `85d5cb7ae436b6b13c6168a05af44e2d57c9e1c8`.
- Ubuntu 26.04.1 LTS, kernel `7.0.0-38-generic`, UEFI with separate writable
  FAT ESP. Ubuntu default verified; Secure Boot unsupported. No installed
  SrvrOS state or pending GRUB one-shot was found.
- Intel Core i5-4260U at 1.40 GHz; 8 GB RAM (7.7 GiB usable).
- Intel `8086:0a26` graphics, subsystem `106b:011a`, Linux i915 boot display;
  connected eDP panel exposes 1366×768.
- APPLE SSD SD0128F, approximately 113 GiB; Marvell `1b4b:9183` AHCI controller.
- Intel `8086:9c31` xHCI; Apple `05ac:0290` revision `0171` topcase.
  Two HID reports captured; interface 2 belongs to Linux bcm5974.
- Analog HDA `8086:9c20`, CS4208 codec `1013:4208`, subsystem `106b:7100`;
  HDMI HDA `8086:0a0c`, codec `8086:2807`.
- Linux applesmc exposes one Exhaust fan. Captured sensor values are Linux
  observations, not SrvrOS telemetry acceptance.
- Internal Broadcom Wi-Fi `14e4:43a0`; attached Netgear USB Wi-Fi `0846:9055`
  revision `0210`, Linux `rtw88_8822bu`; Bluetooth `05ac:828f`.
- Privileged inventory `build/hardware/macbookair6-1-2026-10-22` has zero
  missing/partial reads, including ACPI, HID descriptors, codecs and CPU policy.
  Raw firmware, EDID and device data remain local and ignored.

## Active work and shared dependencies

Bounded objective: provision the documented host toolchain, capture this unit,
validate the unchanged integration baseline and prepare console/input bring-up.
Scope is this record and model registration in current guides. No native device
or firmware gates are widened.

Fetched main at setup. Open PR #52 at `171b307` owns mounted Home battery
return work; PR #19 at `2bc3689` owns Edimax USB Wi-Fi/xHCI diagnostics.
Neither grants this Air support. Avoid competing xHCI edits until a concrete
Air6,1 transport reproducer exists; retain both branches' profile contracts.

The captured 84-byte USB configuration is byte-identical to the existing
Air7,2 `apple-05ac-0291-config.bin` fixture. Keyboard is boot interface 1,
endpoint 0x82, packet 64, interval 8. This supports reuse of configuration
parsing, not native transport or trackpad report acceptance. Current
`parse_trackpad_config` explicitly refuses PID `0290`; descriptor similarity
alone does not establish coordinate ranges, report format or mode switching.
The existing CS4208 routes admit only subsystem `106b:7e00` or `106b:5e00`,
not this unit's `106b:7100`. Backlight and SMC matching also target captured
Air7,2/Pro11,2 firmware. These require independent profiles, not a model rename.

## Capability ledger

| Capability | State | Remaining work |
| --- | --- | --- |
| Boot/display | Not physically tested | Console boot and 1366×768 layout |
| Keyboard | Not physically tested | Native enumeration and typing |
| Trackpad/gestures | PID not admitted | Captured report/transport evidence before profile |
| USB workspace | Prepared; offline checks pass | Physical save/read/unmount pending |
| Sound/routes | Not tested | Exact codec routing evidence and listening checks |
| Brightness | Not tested | Exact firmware/register evidence |
| Temperature/fan | Linux inventory only | Native read-only profile evidence |
| Battery/AC | Not tested | Independent firmware audit; ordinary AML stays off |
| Networking | Not tested | Netgear is distinct from the Pro's Edimax work |

## Host validation

Against the recorded integration base, with documentation changes only:

- Documented Ubuntu dependencies installed; all seven pinned submodules initialized.
- `tools/host-env.sh make check-host`: PASS.
- `tools/host-env.sh make -j2 all usb-image test-core`: PASS; Zig 0.15.2
  verified, boot ISO and USB image produced.
- `make check-docs`: PASS.
- `tools/host-env.sh make smoke`: PASS: syscall ABI, all 481 shell commands,
  shell editing, UEFI USB keyboard and displayd. Final shell filesystem check
  reported zero errors/leaks. The initial sandbox run was blocked by Unix-socket
  permissions; the complete host-access rerun passed.
- These tests validate the integration baseline on this Ubuntu host, not native
  MacBookAir6,1 devices. Exact physical stage rehearsal remains pending.

## Local boot/test handoff

Volatile state lives in `build/bringup/checkpoint.json`; logs in `build/review`.
The 50-file inventory backup was hash-verified outside `build`, under
`~/.local/share/srvros/evidence/macbookair6-1-bootstrap-2026-10-22`.
The user authorized dependency installation using a machine-local sudo credential.
No physical reboot has been requested or armed. Ubuntu remains recovery; there
is no accepted SrvrOS payload to preserve yet. A physical trial requires this
unit's exFAT workspace, exact-stage VM result and recorded recovery/run state.

## Next actions

1. Attach an exFAT USB workspace, identify its USB ancestry and UUID, and run
   `tools/hardware_workspace.py --uuid <observed-uuid>` without formatting.
2. Rehearse the exact v7 console stage, retain its artifacts and record the
   first attended console/input trial before installing/arming it.
3. Capture bounded topcase reports before proposing a separate Air6,1 profile;
   keep backlight, audio, EC/SCI and network enablement independently scoped.

## First console trial prepared (2026-10-22)

Consumed merged main `9ca1922` (PR #52) into clean device source `b31f743`.
PR #53 at `b67d179` continues Air battery desktop ownership; PR #19 at
`2de825f` remains Pro Wi-Fi work. No device gates changed locally.

Rebuilt ISO/USB/core checks and all 51 staging tests pass. Exact v7 console
release `c35475f62f154f1e` passes GRUB chainload, RAM filesystem, USB keyboard,
case/Caps/repeat and renderbench rehearsal. This console harness does not attach
a workspace disk; its default MBR field does not validate this physical GPT USB.

The attached approximately 57.6 GiB exFAT partition on GPT USB storage is now
prepared without formatting. All 102 pre-existing file hashes are unchanged;
workspace starters and a console checklist add five files. Both pre/post
read-only fsck and independent allocation checks pass, with zero leaks or errors.
The selected UUID, paths and file hashes remain in the ignored local run record.

Installed current is `c35475f62f154f1e`, with no previous SrvrOS release.
Ubuntu default and GRUB syntax are verified; no one-shot is armed and no reboot
has occurred. Payload and trial evidence have a hash-verified backup outside
`build`; this is a VM-tested candidate, not physically accepted recovery.
Run record: `build/native-desktop-trials/c35475f62f154f1e/run.json`.

Next is the attended console/keyboard trial, including a saved
`console-keyboard-first.txt`, clean sync/unmount and Ubuntu return. v7 does not
create automatic desktop session logs: retain photographs and user observations,
verify the saved file and original-file hashes, and run offline filesystem checks
before another trial. Physical keyboard, display and USB acceptance remain pending.
