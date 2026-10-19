# Ubuntu setup for an on-device SrvrOS agent

Current workflow, 2026-10-18. Each Mac runs Ubuntu as its development/recovery
OS, has its own repository checkout and agent, and uses a plugged-in exFAT USB
drive for SrvrOS documents and diagnostics. Build and run disposable VMs on
that Mac; reboot it into SrvrOS for physical acceptance. Ubuntu is the normal
boot default. The boot filesystem stays in RAM; persistent work goes to USB.

Read this guide before the dated development journals. Those journals preserve
what was true at each checkpoint, including failed candidates. The
[hardware plan](apple-hardware-expansion-plan.md) describes model differences;
[machine records](machines/README.md) distinguish tested support from plans.

## 1. One checkout and agent per machine

Use `main`, the shared integration baseline. The completed work from
`codex/linux-development-baseline` has been promoted into it. Use a
coordinator-selected commit from `main`, record that commit, and create a
machine branch. For example, on the Pro11,2:

If Git is not installed yet, install the packages in section 2 first; run its
repository commands after cloning.

```sh
git clone --branch main --recurse-submodules \
  https://github.com/PaulMDemers/SrvrOS.git
cd SrvrOS
git rev-parse HEAD
git switch -c device/macbookpro11-2/bringup
```

Use `macbookair7-1`, `macbookpro11-2`, `macbookpro11-3`, `macmini7-1`, or
`macbookair7-2` as the machine slug. Distinguish multiple physical units with a
local label. Do not run concurrent builds/staging from two agents in one checkout:
`build/hardware-boot/latest`, generated images and many VM logs are shared paths.

Give the local agent the repository directory and the handoff prompt at the end
of this guide. Its machine-specific status belongs in `docs/machines/<slug>.md`.
Keep raw captures, run manifests and images local; publish small source changes,
replay fixtures and factual summaries. Push device branches and integrate shared
fixes through review; do not have every agent push to the shared baseline branch.
Avoid duplicate kernel fixes by stating the files/subsystem being changed in the
machine record and coordinating shared work with the integration owner.

## 2. Install Ubuntu build and diagnostic tools

Use x86_64 Ubuntu booted in UEFI mode. CI targets Ubuntu 24.04; this does not mean
every physical Mac or Ubuntu version has passed. From a terminal with the user's
normal sudo access:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential git python3 python3-pil xorriso \
  qemu-system-x86 qemu-system-gui qemu-utils ovmf acpica-tools \
  grub-common grub-efi-amd64-bin mtools dosfstools exfatprogs \
  pciutils usbutils efibootmgr mokutil udisks2 ripgrep
git submodule update --init --recursive
tools/host-env.sh make check-host
```

Zig 0.15.2 and pinned Limine artifacts are downloaded by Make. Use the recorded
submodule revisions, not `git submodule update --remote`. The base image does not
build the full Node runtime; see [development setup](development.md) for the
historical Node port recovery. Internet is needed for initial packages/tools.
The optional `tools/setup_local_host.py` unpacks some VM tools without sudo, but
does not provision GRUB, filesystem utilities or physical boot permissions.

Use normal interactive sudo or the machine's existing approved privilege setup.
Credentials are machine-local; do not copy another agent's password file or put
credentials in Git, captures or handoff notes. Record the user's existing reboot
permission and honor it across turns; a device test cannot run while Ubuntu is
being used for work that must remain running.

Build on the internal Ubuntu filesystem, not the exFAT workspace:

```sh
mkdir -p build/review
tools/host-env.sh make -j2 all usb-image test-core
tools/host-env.sh make smoke
```

Start with `-j2`, especially on 4–8 GB machines. `make smoke` is the baseline
suite, including the older displayd regression; the current desktop has its own
staged test below. Do not run VM scripts concurrently unless their output paths
are isolated. `make clean` removes generated OS products; `make distclean`
removes **all of build**, including local test evidence and staged releases.
Archive needed evidence outside it first.

## 3. Identify this machine and capture Linux evidence

```sh
cat /sys/class/dmi/id/product_name
cat /sys/class/dmi/id/product_version
test -d /sys/firmware/efi
findmnt -M /boot/efi -o TARGET,SOURCE,FSTYPE,OPTIONS
sudo efibootmgr
sudo mokutil --sb-state
sudo python3 tools/capture_linux_hardware.py
```

The collector creates `build/hardware/<timestamp>/`. Read `capture.json`; missing
reads are not absent devices. It includes PCI/USB, ACPI tables, power/backlight,
CPU, sensors and audio codec dumps without consuming input events. Apple HID
report capture enumerates vendor `05ac` across product IDs and records missing
descriptors explicitly. `machine-topology.json` adds board model, PCI bindings,
DRM connectors/modes/EDID, boot-display flags and block-device ancestry. Raw EDID
may identify a monitor; retain raw captures locally. Inspect the actual topcase
descriptors and missing-read list before calling the capture complete. See the
expansion plan.

Record CPU, memory, GPU(s), screen, firmware revision, original/replaced storage,
USB input and audio identities in the machine record. On Pro11,3, preserve the
firmware-selected graphics route; do not guess GMUX or discrete-GPU power writes.
The current desktop surface limit is 1440x900. Retina 2880x1800 scaling and buffer
limits are pending work; booting a framebuffer does not establish usable Retina
support. Do not copy the Air's firmware hash, register offsets or codec quirks to
another model just to make a probe succeed.

## 4. Select the mounted exFAT flash drive

Use an already exFAT-formatted USB volume. The following flow does not format
it. A separate drive per physical machine is recommended; do not clone its UUID
or use the Air's `37D8-C5D0` as a default on another machine. Keep the drive
plugged in before booting SrvrOS; hotplug discovery is not implemented for this
workspace workflow.

```sh
lsblk -o NAME,PATH,TYPE,TRAN,SIZE,FSTYPE,UUID,MOUNTPOINTS,MODEL
```

Identify the intended **partition**, its USB ancestry, and UUID. Set the UUID
from that output in the current shell (replace the example text):

```sh
SRVROS_USB_UUID='YOUR-EXFAT-UUID'
SRVROS_USB_PART="/dev/disk/by-uuid/$SRVROS_USB_UUID"
readlink -f "$SRVROS_USB_PART"
# If not already mounted by Ubuntu Files:
udisksctl mount -b "$SRVROS_USB_PART"
```

Do not substitute a whole disk for its partition. The helper rejects non-USB,
non-exFAT, duplicate UUIDs and absent/ambiguous mounts. Confirm its resolution:

```sh
SRVROS_USB_MOUNT=$(python3 - "$SRVROS_USB_UUID" <<'PY'
import sys
sys.path.insert(0, 'tools')
from hardware_test import volume_id, volume_mount
print(volume_mount(volume_id(sys.argv[1])))
PY
)
printf '%s\n' "$SRVROS_USB_MOUNT"
python3 tools/hardware_workspace.py --uuid "$SRVROS_USB_UUID"
SRVROS_RELEASE=$(cat build/hardware-boot/latest)
```

Preparation verifies built artifacts, creates `srvros-work/.workspace` with the
selected compact UUID and adds starter files only when absent. It preserves
existing documents and rejects a conflicting marker. It stages a **v7 console
workspace** profile and updates `build/hardware-boot/latest`; that is not yet the
desktop profile. It does not install or reboot.

The whole selected exFAT volume becomes writable in SrvrOS. `srvros-work` is an
organizational directory, not an access boundary. `/fat` is the RAM root;
`/work` is the entire selected volume; `/work/srvros-work` is desktop Home.
Other USB volumes are not opted into writes. Keep source code/build tools on
Ubuntu and documents/screenshots/logs on this drive.

## 5. Stage and validate the exact candidate

For a new model, start with console/workspace boot and input enumeration before
trying all desktop providers. Use the release printed by workspace preparation;
its normal VM rehearsal is:

```sh
tools/host-env.sh python3 tools/hardware_boot_smoke.py --console-check
```

This checks boot and console input; the v7 rehearsal does not attach the
prepared physical workspace or establish USB persistence. Use the desktop
rehearsal below for disposable workspace coverage, then validate the real drive.

For the interactive desktop candidate, after a successful build:

```sh
python3 tools/hardware_boot.py stage --desktop-volume "$SRVROS_USB_UUID"
SRVROS_RELEASE=$(cat build/hardware-boot/latest)
cat "build/hardware-boot/$SRVROS_RELEASE/manifest.json"
tools/host-env.sh python3 tools/hardware_boot_smoke.py --desktop-check --audio-check
cat build/review/hardware-grub-result.json
```

The current v22 desktop profile includes gestures, brightness, HDA, CPU/SMC
telemetry and CDC ECM. Hardware matching may leave providers unavailable on new
models. It excludes the optional `srvros.aml=probe`, whose original Air7,2 stall was followed by a clean explicit v24 capture.
Do not re-enable that probe in routine desktop startup. The separate v19
`--laptop-probe-volume` profile is a bounded preflight capture; it is not the
isolated AML interpreter diagnostic.

For a bounded unattended idle sample after the basic workspace is accepted:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --idle-capture
SRVROS_RELEASE=$(cat build/hardware-boot/latest)
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

This distinct v23 profile initializes the same gated device services as v22,
leaves the console idle for 60 seconds, and saves periodic cached scheduler,
thermal, fan and audio status. It blocks the calling thread while waiting;
logging and device sampling still create some activity. It does not launch the
desktop or play audio and never enables AML. Success saves/flushes the exit log,
unmounts the USB workspace and returns to Ubuntu. Missing workspace, incomplete
sampling or failed save/unmount halts. Rehearse the missing-drive failure with
`--probe-check --probe-missing-workspace`; that failure rehearsal does not replace
the successful exact-stage result. Collect using `hardware_desktop.py` and require
this release's exit log containing `idle-capture: ... completed=yes`, plus a healthy
collected session and verified unmount. Scheduler timer samples are
coarse occupancy evidence, not a wattmeter or proof of desktop responsiveness.
For a real desktop sample, use the separate v25 profile after desktop boot and
USB saving are known to work:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --desktop-idle-capture
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

It opens Text, allows startup to settle, samples CPU0 scheduler counters for
60 seconds, saves a screenshot to the selected workspace and closes the desktop.
Clean log save/unmount then returns to Ubuntu; failure halts. Leave input alone
during this measurement. This is an explicit unattended diagnostic, not a timed
exit added to ordinary desktop use. The collector's `desktop_idle_sample` requires
the complete measurement, screenshot success and clean process return. The
exact-stage VM rehearsal also reads and decodes the saved BMP from the disk image.
The sample excludes startup and screenshot writing, but includes normal Text
caret/clock updates and periodic USB/device logs. It measures scheduler occupancy,
not energy consumption, CPU frequency, equilibrium temperature or interactive
input acceptance. Keep the physical result separate from the console-idle sample.

Use the same recorded recovery/one-shot installation sequence below; keep the
accepted previous desktop when the current payload is only an automated test.

For explicit AML diagnostics, the separate v24
profile uses `--laptop-probe-volume "$SRVROS_USB_UUID" --aml-capture` and the
same `--probe-check` VM rehearsal. It starts USB logging before the snapshot-only
probe and returns only after clean completion and save/unmount. This is not a
normal desktop profile or a battery service. A kernel/interpreter stall can
still require manual recovery; only run it in an authorized recovery window.
Never silently substitute it for the idle capture or ordinary desktop.

The desktop VM harness uses disposable disks, tests two fresh boots, USB saves,
preferences and logs, and checks emulated audio. It reads `latest`, so do not
stage anything between the candidate and its rehearsal. In particular,
`make hardware-smoke` stages a different console profile. Check the PASS result
contains **this exact release**; archive logs before another test overwrites them.
VM success does not accept physical sound, sensors, topcase or GPU routing.

Before rebooting, create a local record under
`build/native-desktop-trials/<release>/` with model, branch/commit, dirty status,
workspace UUID, release manifest, VM results, intended checks, current/previous
installed release and previous USB session IDs. Copy the screenshots after the
run. Keep a durable copy of accepted boot payloads outside `build` before cleanup.
Shell variables do not survive reboot: retain this record and reload UUID/release
from it on return. Do not rely on an old `latest` pointer as a machine identity.

## 6. Install alongside Ubuntu and select one boot

The installer requires UEFI Ubuntu, a separate writable FAT ESP at `/boot/efi`,
GRUB tools, and Secure Boot disabled or unsupported. It verifies artifacts,
creates `EFI/srvros/<release>`, manages `/etc/grub.d/42_srvros`, regenerates GRUB
and retains a previous entry when available. It does not change the firmware
boot order or Ubuntu's default and does not reboot by itself.

Read the current host state before installing:

```sh
sudo cat /boot/efi/EFI/srvros/state.json  # Absent on a first installation.
sudo grub-editenv /boot/grub/grubenv list
cat /etc/default/grub
```

If `next_entry` already names a pending boot, reconcile it with the local run
record before replacing it. Do not assume an earlier test has returned merely
because the agent's conversation resumed.

On a first install there is no SrvrOS recovery release. On subsequent installs,
the old current becomes previous by default. If **previous already is the
accepted recovery**, add `--keep-previous` to retain it; that option fails on a
first install. Do not promote an untested current build over the last accepted
recovery. Keep an independently verified copy of accepted artifacts: future
installs can remove the obsolete previous release. ESP space must cover the new
payload plus 32 MiB; archive and verify unreferenced payloads before freeing any
space, never another OS loader or an active recovery entry.

```sh
sudo python3 tools/hardware_boot.py install --release "$SRVROS_RELEASE"
sudo grub-script-check /boot/grub/grub.cfg
```

Before arming a reboot, verify the expected simple Ubuntu return configuration:

```sh
sudo python3 - <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'tools')
from hardware_test import check_ubuntu_default
check_ubuntu_default(Path('/etc/default/grub').read_text(),
                     Path('/boot/grub/grub.cfg').read_text())
print('Ubuntu default verified')
PY
```

A mismatch needs a host-specific boot plan; do not silently rewrite the default.
With reboot authorized, work saved and the flash drive attached:

```sh
sudo grub-reboot srvros-current
sudo grub-editenv /boot/grub/grubenv list
sync
sudo systemctl reboot -i
```

`-i` bypasses GNOME/user-session inhibitors. Use it only for the intended reboot.
GRUB consumes `next_entry=srvros-current` once, leaving Ubuntu for the following
boot. If a reboot is cancelled or rejected, clear the pending selection:

```sh
sudo grub-editenv /boot/grub/grubenv unset next_entry
```

A first boot can instead be selected manually from the GRUB menu. The helper is
not a macOS boot installer, and no raw USB image needs to be written over the
exFAT logging drive for this Ubuntu/GRUB workflow.

## 7. Test, return and collect

The desktop profile opens persistent Help instructions and has **no timed exit**.
Do the recorded tests; save screenshots with Command-Shift-3. Return through
SrvrOS → Return to console, check `USB unmounted=yes`, then type `reboot` at the
monitor. An unmount/write failure is not a successful exit. For a console
workspace use `workspace sync`, `workspace unmount`, then `reboot`.

On a halt, photograph the complete diagnostics before manually restarting. After
Ubuntu returns, mount the same volume and reload its UUID/release from the local
record. Collect before preparing another run:

```sh
python3 tools/hardware_desktop.py --uuid "$SRVROS_USB_UUID" \
  --output "build/native-desktop-trials/$SRVROS_RELEASE/physical-after"
```

This verifies and copies session logs, not screenshots or documents. Copy the
relevant BMPs from `srvros-work/Screenshots` separately and verify their hashes.
Match a new session's release to the staged manifest, inspect `attention`, exit
status and device/sample-age fields, and record what the human actually heard or
felt. Old sessions are included in collection; their presence is not a pass for
the new build. No new session after a boot stall may mean failure before logging.
No agent resumes automatically after reboot merely because these scripts exist;
reopen its task and tell it Ubuntu has returned if it has not reconnected.

For an offline, read-only filesystem check, first finish collection, close open
Files/terminal views of the volume and unmount it:

```sh
udisksctl unmount -b "$SRVROS_USB_PART" && sudo fsck.exfat -n "$SRVROS_USB_PART"
udisksctl mount -b "$SRVROS_USB_PART"
```

Do not run repair flags automatically after a failed test. Preserve the failure
report and investigate the write path. A clean check establishes filesystem
consistency at that point, not power-loss atomicity.

## 8. Unattended filesystem confidence tests

Once boot/input and the workspace work on that machine, use a fresh request:

```sh
python3 tools/hardware_test.py prepare --uuid "$SRVROS_USB_UUID" --stress
SRVROS_RUN=$(cat build/hardware-tests/latest)
SRVROS_RELEASE=$(cat build/hardware-boot/latest)
tools/host-env.sh python3 tools/hardware_test_smoke.py --release "$SRVROS_RELEASE"
# Record the printed run/release and preserve the accepted recovery first.
python3 tools/hardware_test.py run --run "$SRVROS_RUN"
# After returning to Ubuntu and remounting the drive:
python3 tools/hardware_test.py collect --run "$SRVROS_RUN"
```

`run` installs its own exact profile, arms GRUB and reboots immediately. Unlike
manual installation with `--keep-previous`, this launcher promotes the old
current to previous; independently preserve recovery first. Five successful
rounds save CRC-checked reports and return automatically. A failed/incomplete
round halts indefinitely for a photograph. `STARTED` without a valid `COMPLETE`
marker is incomplete, even if the machine returned automatically or a file exists.
Collection also checks retained file contents. These tests do not establish
physical key latency, right-click behavior or audible sound. See
[automated hardware tests](automated-hardware-tests.md).

The exact-release VM rehearsal uses disposable requests and a matching UUID;
the physical requests remain unused. Confirm its `autotest-cycle-result.json`
matches the intended release, and archive that evidence before another harness
run replaces it.

## 9. Agent handoff and integration contract

Use this starting prompt, filling in the machine and the scope:

> Work on SrvrOS locally on this Ubuntu Mac. Read
> `docs/ubuntu-device-agent-setup.md`, `docs/machines/README.md`, this model's
> record, and `docs/apple-hardware-expansion-plan.md`. Confirm model, branch,
> source commit and attached USB UUID before making host changes. Develop on
> this device's branch and keep its record current. Preserve Ubuntu and the
> last accepted local recovery release. Reuse shared drivers and add only
> evidence-backed device profiles. Validate in disposable VMs, then perform
> authorized physical tests. Record each exact release and intended checks
> before reboot; collect logs/screenshots after return, including failures.
> Honor the user's existing sudo/reboot authorization and availability; do not
> assume another machine's permission, passwords, UUIDs or test results apply.
> Publish small reviewable changes with tests, device IDs and remaining limits.

The local agent owns its machine's inventory, boot/recovery state, diagnostics
and hardware acceptance. Shared input, filesystem, scheduler, UI and protocol
changes need coordination and regression on the original Air. Model-specific
success is not a pass for siblings. Prefer separate commits for a generic fix
and new device matching. Keep failed captures and document rollback/recovery;
never relabel an incomplete test as a pass to advance the milestone.
