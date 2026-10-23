# Ubuntu setup for an on-device SrvrOS agent

Current workflow, 2026-10-19. Each Mac runs Ubuntu as its development/recovery
OS, has its own repository checkout and agent, and uses a plugged-in exFAT USB
drive for SrvrOS documents and diagnostics. Build and run disposable VMs on
that Mac; reboot it into SrvrOS for physical acceptance. Ubuntu is the normal
boot default. The boot filesystem stays in RAM; persistent work goes to USB.

Read this guide before the dated development journals. Those journals preserve
what was true at each checkpoint, including failed candidates. The
[hardware plan](apple-hardware-expansion-plan.md) describes model differences;
[machine records](machines/README.md) distinguish tested support from plans.

Read the [coordination guide](agent-coordination.md) for ownership, overlapping
changes, PR integration and checkpoints across machines.

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

Use `macbookair6-1`, `macbookair7-1`, `macbookpro11-2`, `macbookpro11-3`, `macmini7-1`, or
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

`cpu-policy.json` records Linux CPU topology, online sets, idle driver/governor
and per-CPU states, plus each frequency policy's membership, governor and limits.
It reads an explicit attribute allowlist without changing policy. Missing
optional fields are omitted; failed reads are null and listed in `capture.json`.
The snapshot has start/end times and an online-set recheck; it is not atomic or
an energy measurement. Capture it with power baselines, and retain the raw file
locally. See [CPU power telemetry](power-telemetry.md#linux-policy-baseline).
`make test-linux-inventory` runs portable fixture checks without privileged
hardware access and is included in `test-core`.

Record CPU, memory, GPU(s), screen, firmware revision, original/replaced storage,
USB input and audio identities in the machine record. On Pro11,3, preserve the
firmware-selected graphics route; do not guess GMUX or discrete-GPU power writes.
Desktop client surfaces remain bounded to 1440x900 logical pixels. An explicit
`deskd --scale=2` path now passes 2880x1800 VM checks; ordinary staging remains
1×. See [display scale](native-desktop.md#display-scale) for the launch and capture
contract. This is integer pixel expansion, not native-density text or accepted
Retina hardware support. Do not copy the Air's firmware hash, register offsets or codec quirks to
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

USB storage discovers validated primary GPT maps as well as primary MBR
partitions. GPT discovery checks both primary checksums, usable ranges, duplicate
GUIDs and overlaps before publishing partitions. It accepts a saturated
protective-MBR length but rejects hybrid maps, unsupported read-only attributes,
tables over 64 KiB and more than 32 used entries. It does not repair GPT or fall
back to its backup header. Rehearse the actual layout with
`hardware_boot_smoke.py --probe-check --workspace-table gpt` for a staged
unattended probe (omit the option for the default MBR fixture). A VM pass is not
physical USB persistence acceptance.

If SrvrOS mounts the volume but refuses writes, inspect the full output of an
offline `fsck.exfat -n` check after unmounting it in Ubuntu. A zero exit status
and a final clean summary can coexist with an invalid bitmap-size diagnostic.
Back up files and affected filesystem metadata before repairing; verify the
checker output and existing file hashes afterward. Do not relax the kernel's
write refusal to bypass a filesystem inconsistency.

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

On the captured Pro11,2 firmware, the laptop probe now reads only the four
CPU/PCH PWM registers after exact GPU/HM87/DSDT matching. This diagnostic does
not enable brightness or sound and does not access EC ports. Use the v19
`--laptop-probe-volume` profile and exact GPT rehearsal for its unattended
save/unmount/return capture; retain the accepted desktop as recovery.

After the Pro's native read-only capture, its explicit brightness trial uses
`--desktop-volume "$SRVROS_USB_UUID" --desktop-scale 2 --desktop-brightness`.
This v30 profile preserves 2880x1800/2x, enables the exact matched brightness
provider and leaves audio/AML/SMC off. Rehearse with `--desktop-check` and the
actual workspace partition-table type. Startup does not adjust the panel;
Settings offers 5–100% only when the captured mode and period still match.
Physical visible adjustment remains a separate acceptance check.

The subsequent Pro audio trial adds `--desktop-audio` to that command (v31).
It retains brightness and scaled display while enabling the exact 8c20/CS4208
5e00 codec profile, including the main and bass speaker converters. Use
`--desktop-check --audio-check --workspace-table gpt` for the exact scaled
rehearsal. That path verifies one captured test tone after desktop exit; it does
not claim the unscaled harness's two-boot or bundled-WAV coverage. Speakers,
headphone isolation and audible quality require the on-screen listening checks.
Startup is silent; no optional AML, EC, SMC or HDMI audio is enabled.

For the captured MacBookAir6,1, use `--desktop-volume "$SRVROS_USB_UUID"
--desktop-basic --desktop-brightness --desktop-audio` for the distinct v57
attended audio trial. It keeps the unscaled desktop and exact brightness profile,
adding only analog HDA. The codec subsystem/revision/pin defaults are independently
gated. Battery/AML and other optional laptop services remain off. See the
[Air6,1 listening checklist](macbookair6-1-audio.md); basic speaker playback is
accepted, with headphones and sustained playback pending.
Use `--desktop-check --audio-check --workspace-table gpt` for its exact-stage VM.

The current v22 desktop profile includes gestures, brightness, HDA, CPU/SMC
telemetry and CDC ECM. Hardware matching may leave providers unavailable on new
models. It excludes the optional `srvros.aml=probe`, whose original Air7,2 stall was followed by a clean explicit v24 capture.
Do not re-enable that probe in routine desktop startup. The separate v19
`--laptop-probe-volume` profile is a bounded preflight capture; it is not the
isolated AML interpreter diagnostic.

The separate v54 **ordinary Home battery session** is an explicit opt-in:
`--desktop-volume "$SRVROS_USB_UUID" --desktop-battery`. Build the full native
battery runtime first and validate the exact stage with
`battery_home_desktop_smoke.py --boot-session --case profile-refusal --stage RELEASE`.
It starts ordinary apps with no capture timer or automatic reboot. Exit with
Ctrl+Alt+Esc, reopen with `desktop battery`, or use monitor `reboot` for a flushed,
unmounted return. This path is physically accepted on the recorded Air7,2
release; default startup stays off and siblings do not inherit its ownership. Follow the
[session contract](battery-home-lifecycle.md#opt-in-ordinary-home-startup) and
existing recovery/run-record steps before a hardware trial.

The v55 combined profile adds `--desktop-services` to `--desktop-battery`.
It enables the existing brightness, HDA, read-only CPU/SMC and ECM providers,
without scaling or an isolated AML probe. Use `--services` with the ordinary
battery VM harness, including its exact-stage run. Combined physical acceptance
is recorded for Air7,2 release `9aa8dbf6fbda8252`; see
[device parity](device-parity.md) for its scoped evidence and the attended checklist. Unsupported providers stay unavailable. Boot-scoped device
workers remain available after desktop exit; app PCM, battery ownership and
session logging retire before the Home hold releases.

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
sampling or failed save/unmount returns with failed/incomplete evidence in new
`-auto-return` stages. Older recipes retain their halt behavior. Rehearse the missing-drive failure with
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
Clean log save/unmount then returns to Ubuntu; new unattended stages also return
on handled failure. Leave input alone
during this measurement. This is an explicit unattended diagnostic, not a timed
exit added to ordinary desktop use. The collector's `desktop_idle_sample` requires
the complete measurement, screenshot success and clean process return. The
exact-stage VM rehearsal also reads and decodes the saved BMP from the disk image.
The sample excludes startup and screenshot writing, but includes normal Text
caret/clock updates and periodic USB/device logs. It measures scheduler occupancy,
not energy consumption, CPU frequency, equilibrium temperature or interactive
input acceptance. Keep the physical result separate from the console-idle sample.

For explicit read-only package energy and CPU0 active-frequency diagnostics,
add `--power-telemetry` to `--desktop-idle-capture` (separate immutable v26 profile).
It is refused for other capture types and does not enable AML or CPU controls.
Use the same exact-stage VM and missing-workspace rehearsals. An emulated CPU
reports power unavailable and the VM result explicitly records that limitation;
success of the capture/save/return workflow is not power measurement acceptance.
On hardware, require a non-null `power_idle_sample`, inspect its covered duration
and raw checkpoints, and review attention flags before accepting telemetry. The
covered interval lies inside the desktop measurement and excludes screenshot
writes. Missing, stale, rejected or refused readings produce no power result.
See [power telemetry](power-telemetry.md) for model gates, units and limitations.

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

The Air7,2 native battery candidate uses a separate **v31** profile and kernel:

```sh
tools/host-env.sh make kernel initramfs acpi-battery-kernel
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --battery-capture
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

This is an explicit firmware/EC/SCI diagnostic, not ordinary desktop startup.
The newly staged native image must refuse Q35, save the refusal, unmount and reset;
the synthetic positive lifecycle uses another kernel and cannot grant physical
acceptance. On the matched Air it takes five battery/AC samples, retires the
runtime, saves and returns automatically, including a failed result. Keep AC connected for
the initial trials. The current diagnostic preserves the two observed firmware
graphics/wake-alarm events while it owns the battery event; unknown enabled
sources still refuse. Unattended battery captures checkpoint and unmount USB
before entering the runtime; a contained fault resets without unsafe cleanup.
Missing final reports remain failures. The last fault text may not reach USB.
Read the [runtime gate](native-ec-runtime.md#explicit-native-battery-diagnostic),
retain recovery, record the exact run and collect its matching session first
on return. The collector reports complete runtime evidence separately from
physical battery accuracy. Pro11,2 desktop profiles v27–v31 remain reserved on its
work branch; do not reuse a profile number or alter an installed recipe.

After the bounded AC-connected diagnostic, the separate **v32 laptop-probe**
profile adds a guided transition trial:

```sh
python3 tools/hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID" --battery-transitions
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check
```

Start with AC connected and leave USB plugged in throughout. Follow the persistent
instructions to unplug AC and then reconnect it. Each phase needs three coherent
readings within 120 seconds; a missed phase saves an incomplete result and halts.
Successful retirement, save and unmount return automatically. Collect the exact
release/session and inspect `battery_transitions`, both event restorations and
attention items before claiming acceptance. Preserve the accepted v31 diagnostic
and ordinary desktop recovery. See the [transition contract](native-ec-runtime.md#guided-ac-transition-capture)
for sample validity, current signs and the distinction between VM and hardware evidence.

The separate **v40 laptop-probe** `--battery-desktop` profile runs the native
cached service alongside a RAM-only compositor study for one minute. Keep AC and
USB connected; no input is required and it returns automatically. The workspace
is checkpointed and parked during overlap; this profile does not use or replace
`battery-fs.img`. Use the native build, exact-stage `--probe-check`, install with
accepted recovery retained, and one-shot/collection sequence below. Require the
matching session's `battery_desktop` proof and clean offline filesystem checks.
VM input or generated redraws do not accept physical key/trackpad latency. See
[the capture contract](battery-service.md#ram-compositor-capture-v40). v39 is
reserved by the separate Pro Wi-Fi probe.

The separate **v38 laptop-probe** `--battery-filesystem` profile exercises an
isolated disposable filesystem image during battery sampling. Bounded native
acceptance is recorded for Air7,2; desktop/input coexistence remains pending.
Prepare a fresh `battery-fs.img` in this machine's `srvros-work`
directory with `python3 tools/battery_filesystem_image.py /your/mounted/volume/srvros-work/battery-fs.img`.
The tool exclusively creates a regular file; it refuses an existing path. Do not
replace a used image before collecting and archiving its associated session.
Then stage `--laptop-probe-volume "$SRVROS_USB_UUID" --battery-filesystem` and run
the exact-stage `tools/hardware_boot_smoke.py --probe-check` before installation.
Build `build/tests/exfat-driver` for post-return collection. Keep AC and USB
connected; the trial needs no input and returns automatically. Follow the
[isolation and recovery contract](battery-service.md#isolated-filesystem-image-trial-v38)
and retain both outer-volume preflight checks. Pro desktop v37 is a separate
profile; do not replace or renumber it when merging these staging changes.
With the selected partition unmounted, also run
`sudo python3 tools/check_exfat_allocation.py --uuid "$SRVROS_USB_UUID" --battery-image`
to reject a fragmented, aliased or incompletely initialized outer file before
reboot. This checks the outer extent; the kernel separately validates the inner
filesystem identity, marker and allocation graph before enabling its view.
A newly created file can still be fragmented; do not bypass this refusal. On
Air7,2 the first unused fixture occupied a one-cluster hole plus a larger run.
After archiving its bytes, retaining that unused allocation while writing a fresh
copy, then removing the temporary copy, produced an admissible contiguous extent.
Only apply such preparation to a disposable fixture whose bytes and lack of a
pending run have been verified; rerun both offline checks afterward. Used fixtures
must first be collected and archived. Never repair or relocate unrelated files to
make this diagnostic run.

The separate **v36 laptop-probe** `--battery-usb-write` profile adds bounded
scratch-data writes/flushes during battery sampling. It allocates and validates
a new 4096-byte scratch file before runtime, unmounts the workspace, and retains
only a private capability to that data extent. General USB write aliases stay
disabled. Use the same native build, exact-stage `--probe-check`, missing-workspace
rehearsal and automatic-return installation sequence. Collection must verify
both `battery_service` and `battery_usb_write`, including the saved scratch
payload; an incomplete report is failure. Keep AC/USB connected; no input is
requested. See [the scratch contract](battery-service.md#native-scratch-writeflush-overlap-capture-v36).
This does not accept concurrent filesystem metadata updates. The Pro11,2 branch
owns the separate v35 CPU-load proposal; do not reuse its profile number.

The separate **v34 laptop-probe** `--battery-usb-read` profile extends the service
trial with uncached reads of the validated, unmounted USB device while battery
sampling runs. It leaves USB writes revoked until clean retirement, verifies
each read against a baseline, and requires sampling progress inside storage
scopes. It is automatic; keep AC and USB connected. The same exact-stage
`--probe-check` rehearsal must verify native refusal and automatic return.
See [the read-overlap contract](battery-service.md#native-read-only-usb-overlap-capture-v34).
On collection require both `battery_service` and `battery_usb_read` evidence.
A missing final report remains failure. This does not accept concurrent writes
or human input responsiveness.

The **v33 laptop-probe** `--battery-service` profile runs a bounded automatic
cached-reader/start/stop test with AC and USB left connected. Follow the
[service guide](battery-service.md) and the same exact-stage/install/one-shot
sequence. New stages use the `-auto-return` recipe suffix, checkpoint/unmount
before risky runtime work and return on handled failures as well as success.
Full final logs are saved after normal retirement; contained faults retain the
pre-probe checkpoint and reset without touching the filesystem. Its
`battery_service` collection field does not accept desktop responsiveness or a
permanent background service.

Announce required human actions before rebooting. Use `--hold-on-failure` only
for an explicitly attended diagnostic. Existing installed recipes keep their
original behavior; restage to obtain automatic failure return. The guided v32
AC transition trial remains attended and must be announced in advance.

### Mounted Home battery boundary (Air diagnostic)

After the parked battery/filesystem captures, the explicit v48 `--battery-home`
profile keeps Home mounted under the battery owner's mount hold. It pauses
periodic log writes, takes fresh samples and retires the service before saving
and unmounting. It does not launch desktop apps or enable normal monitoring.
Use `stage --laptop-probe-volume "$SRVROS_USB_UUID" --battery-home`, then the
exact `--probe-check` and `--probe-check --probe-missing-workspace` rehearsals.
No input is required. Normal return or handled refusal saves and returns to
Ubuntu; uncertain live runtime state halts, with no filesystem cleanup/reset
claim. The exact Air firmware gates remain in force; unsupported models refuse.
Air7,2 release `71ab6b95892a9a8c` accepted this bounded no-client capture with
fresh samples, clean retirement, unchanged unrelated files and automatic return.
Desktop-session ownership and ordinary startup remain pending. See
[Home lifecycle](battery-home-lifecycle.md) for proof and remaining acceptance.
The subsequent v49 `--battery-home-session` capture adds explicit System-control
lifetimes and Text file work with Home held. It requires a fresh dedicated
`battery-study` directory as described in that guide, exact-stage VM validation,
and its own physical acceptance. The ordinary desktop recipe is unchanged.
Air7,2 release `fd6f6b9d397c339f` accepts the bounded v49 session with all five
file cycles, three service lifetimes and clean retirement/automatic return.
This does not enable normal startup or accept physical charging/gauge accuracy.
Use the collector containing the `7f1438d` session-identity fix; earlier v49
collection can mislabel the JSON session while leaving its saved logs intact.

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

The `srv>` monitor is not the userspace shell: `echo text > file` prints the
redirection literally. After `workspace shell`, require a prompt ending in `$`
before using shell redirection; if it stays at `srv>`, record the error and
check `workspace status`. For a monitor-only save check on a writable workspace,
use `write /work/srvros-work/console-test.txt test-text` followed by
`cat /work/srvros-work/console-test.txt`, then sync/unmount as above. Use a fresh
filename and verify its contents after returning to Ubuntu.


On a halt, photograph the complete diagnostics before manually restarting. After
Ubuntu returns, mount the same volume and reload its UUID/release from the local
record. Collect before preparing another run:

```sh
python3 tools/hardware_desktop.py --uuid "$SRVROS_USB_UUID" \
  --output "build/native-desktop-trials/$SRVROS_RELEASE/physical-after"
```

This verifies and copies session logs, not screenshots or documents. Copy the
relevant BMPs from `srvros-work/Screenshots` separately and verify their hashes.
A missing exit snapshot is flagged as an incomplete session, including when a
halt occurs before any failure text reaches USB. Match a new session's release
to the staged manifest, inspect `attention`, exit
status and device/sample-age fields, and record what the human actually heard or
felt. Old sessions are included in collection; their presence is not a pass for
the new build. No new session after a boot stall may mean failure before logging.
No agent resumes automatically after reboot merely because these scripts exist;
reopen its task and tell it Ubuntu has returned if it has not reconnected.

For an offline, read-only filesystem check, first finish collection, close open
Files/terminal views of the volume and unmount it:

```sh
make build/tests/exfat-driver
udisksctl unmount -b "$SRVROS_USB_PART"
sudo fsck.exfat -n "$SRVROS_USB_PART"
sudo python3 tools/check_exfat_allocation.py --uuid "$SRVROS_USB_UUID"
udisksctl mount -b "$SRVROS_USB_PART"
```

Require successful results from both checks. `fsck.exfat -n` alone can report
clean while the allocation bitmap contains orphaned clusters: this occurred on
the Air with exfatprogs 1.3.2. The additional helper runs the real SrvrOS
allocation checker with an `O_RDONLY` descriptor and no write/flush callbacks.
It verifies USB partition identity and refuses mounted volumes. Exit 0 means
the supported allocation graph passed, 1 means it failed, and 2 means it could
not be checked. Mount or capacity refusal is not a pass. This complements fsck;
it is not a complete filesystem certification or power-loss atomicity test.

Do not run repair flags automatically after a failed test. Preserve the failure
report and investigate the write path. For proven orphan allocations, first
preserve boot/FAT/all allocated clusters outside `build`, rehearse
`fsck.exfat -y -s` on a disposable copy, and verify every existing file's bytes and
the recovered `LOST+FOUND` contents. Recheck unchanged source media before any
explicit recovery. Never clear allocation bits merely to pass a scratch test.
The Air's [recovery record](machines/macbookair7-2.md#allocation-check-discrepancy--october-20-resolved)
does not authorize applying that repair to another machine's unexplained error.
The [exfatprogs rescue implementation](https://github.com/exfatprogs/exfatprogs/blob/1.3.2/fsck/fsck.c)
handles orphan recovery separately from ordinary read-only checking.

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

Use the [coordination guide](agent-coordination.md) alongside this prompt; publish
the new unit's bounded scope and dependencies before shared implementation.

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

### Guided keyboard acceptance

Stage `hardware_boot.py stage --laptop-probe-volume "$SRVROS_USB_UUID"
--keyboard-capture` for the opt-in v28 guided trial. Rehearse with
`hardware_boot_smoke.py --probe-check --workspace-table gpt`; add
`--keyboard-fail` to exercise wrong-key recovery or `--keyboard-timeout` to
verify a saved deadline failure.
The screen prompts for lowercase, Shift, Caps Lock on/off, Backspace, Enter and
all four arrows. Each step has 90 seconds. Only keyboard input counts. Unexpected input repeats
the current prompt without resetting its deadline; a timeout records FAIL. Completed and timed-out trials save logs, flush/unmount and
return automatically if storage succeeds. Missing/failed storage still halts.
Use the built-in keyboard only for its acceptance. A successful VM rehearsal
or merely enumerating a keyboard is not physical typing acceptance.

### Retina desktop trial

For an explicit 2880x1800 framebuffer and a 1440x900 logical desktop, stage
`hardware_boot.py stage --desktop-volume "$SRVROS_USB_UUID" --desktop-scale 2`
(v29). Rehearse that exact payload with `hardware_boot_smoke.py --desktop-check
--workspace-table gpt`. This profile enables trackpad gestures and read-only
laptop diagnostics; it does not enable optional AML, audio or SMC probing.
The opening Help page displays pointer checks and keyboard-only exit instructions.
There is no deadline or automatic reboot. Save work, choose Return to console
(F10, End, Return), and type `reboot` only after `USB unmounted=yes`.

The `05ac:0262` trackpad candidate matches the captured vendor-HID interface 1,
endpoint 0x81/64-byte packets, interval 2. It uses an explicit Wellspring7 Type2
decoder and feature mode switch, separate from the Air0291 Type3 profile.
Synthetic protocol/transport tests do not accept physical gestures. Keep the
last accepted keyboard payload and collect the native session before proceeding.

### Prompted Ethernet cable checks

With `--desktop-network`, the hardware desktop opens the automated Terminal
trial. Connected diagnostics and report saving start automatically. Unplug only
the Ethernet cable when prompted, press Enter, then reconnect and press Enter at
the next prompt. Both gates wait indefinitely. Leave USB devices connected.
Reports are saved in Home (`network-before.txt`, `network-down.txt`,
`network-after.txt`, sequence state in `network-trial.txt`). Inspect statuses,
HTTP responses and System/Network Utility behavior; completion alone is not a
pass. The final screen gives the clean console-return and reboot instructions.


### Automatic Text and battery overlap

The captured Air7,2 can use the opt-in v41 `--battery-text` recipe after local
validation and exact-stage rehearsal. It runs a one-minute RAM Text editing
script with cached battery sampling, then saves and returns automatically. No
human input is needed; leave keys/trackpad alone and keep AC/USB attached. The
collector requires exact editing and retirement proof, not merely a reboot.
Follow the [battery capture contract](battery-service.md#native-automatic-text-capture-v41)
and retain the accepted desktop recovery. Other models must retain their native
refusal until independently matched and accepted.

### Automatic battery Start/Stop study

The captured Air7,2 can stage v44 with `--laptop-probe-volume <local-volume-id>
--battery-controls`. Build the native battery kernel and matched userspace first,
then follow the same exact-stage rehearsal, recovery preservation, run record and
one-shot boot sequence. This is unattended: keep AC/USB connected and leave input
alone. System starts, samples and stops twice; the outer workspace stays parked.
The capture saves logs and returns automatically, including handled failures.
A contained fault can return without final logs and must be recorded as incomplete.
Collect the matching `battery_controls` proof and verify unchanged prior USB files
and offline allocation before accepting the run. v44 does not replace or use the
old disposable filesystem image. See the [control contract](battery-service.md#scoped-asynchronous-controls-in-development).
Ordinary desktop battery startup and other models' native gates remain unchanged.


### Automatic parked battery query capture

The reference Air7,2 agent may stage `--laptop-probe-volume <this-host-volume-id>
--battery-query` after building `acpi-battery-kernel`. This v45 profile requires
an existing workspace and automatic return; it cannot be staged with hold-on-failure.
Follow the normal exact-stage VM, recovery, saved run record, install and one-shot
boot sequence above. No typing, AC change or other human action is required.
It waits up to 30 seconds for a natural unsupported EC query while USB is parked,
then retires and saves before returning. A no-event run is not exercised, not a
pass. A contained fault may reset without a final report; collect and treat that
as incomplete/failure. Require the collector's `battery_query` proof plus USB
preservation/offline checks before physical acceptance. Ordinary startup remains
off; other models must not reuse the Air firmware gate or acceptance.


## Parked SMBus host event capture (Air7,2 v46)

After the query-refusal baseline, the explicit `--battery-smbus` staging option
adds bounded natural host-controller event handling while the outer USB volume
is parked. See the [ownership and proof contract](battery-smbus-events.md#parked-native-capture-v46).
It is automatic and has no human interaction. Use this unit's UUID, exact native
stage refusal/save/return and missing-workspace VM checks, recovery copies and
normal preflight/collection sequence. The collector's `battery_smbus` field must
be present and valid; a quick reset or start log alone is not a pass. Physical
gauge accuracy, mounted Home and ordinary startup remain separate acceptance.
This profile adds no other machine's firmware gate or accepted capability.

Air7,2 release `a1451825851dc5f0` accepted natural completion events, fresh
sampling, clean retirement and saved automatic return with preserved files and
clean offline allocation. No alarms occurred; do not count an absent alarm as
acknowledgement or cache-invalidation acceptance. Keep event counts and physical
accuracy limits in the machine record rather than promoting all VM cases to
hardware passes.

## Parked SMBus with Text/file activity (Air7,2 v47)

The separate `--battery-smbus-files` recipe uses the native diagnostic kernel,
automatic return, and a fresh exclusive `battery-fs.img`. Follow the image
preparation, exact-stage and backup steps above; preserve the prior diagnostic
image before replacing it. See the [ownership contract](battery-smbus-events.md#parked-smbustextfile-capture-v47).
Collect `battery_smbus_files`, including `image_verified`, and independently
check outer-file preservation, both filesystems/allocation, and Ubuntu recovery.
A valid v46 receipt does not accept v47. No physical interaction is required;
missing completion, interrupted work, event-limit failure or quick reset is not
a pass. Ordinary mounted Home and automatic battery startup stay off.

### Automatic continuous Home battery trial

For the reference Air's explicit continuous desktop trial, build both the normal
image and native diagnostic kernel, then stage with
`python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_VOLUME --battery-home-desktop`.
This selects v51 and automatic return. Use the normal exact-stage VM, durable
payload/evidence backup, verified install and one-shot Ubuntu recovery sequence
above. The separate Q35 capture fixture validates the positive path; exact-stage
VM rehearsal must refuse native hardware ownership and return cleanly.

The run needs no input. It opens ordinary Home/Text for three minutes while the
battery service and periodic logging run, saves a Home/Screenshots BMP, retires
all inherited clients and firmware ownership, saves `exit.log`, unmounts and
returns. Existing Home files and preferences must be preserved. No fresh
`battery-study` directory is needed. Allow several minutes for boot, screenshot
I/O and retirement. Only unresolved live ownership may halt; a clean refusal
returns automatically and is not a pass.

Collect the matching session before another run. Require the strict
`battery_home_desktop_records.check_capture` result plus the new screenshot,
source/release identity, prior-file hashes and offline filesystem checks.
Physical typing and AC/gauge accuracy remain separate attended acceptance.
See [the Home lifecycle contract](battery-home-lifecycle.md#explicit-automatic-home-capture-v51).


The reference Air7,2 accepted v51 release `8666698f5e5d4ed5` with a complete
201-second native session, periodic/final logs, screenshot and clean filesystem
retirement. This accepts the explicit automatic workload on that unit only;
ordinary startup, attended AC transitions and gauge accuracy remain separate.

### Attended Home AC transition capture

For the next battery/indicator checkpoint, stage
`python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_VOLUME --battery-home-transitions`.
This is v52, separate from v51's unattended idle capture. Validate the host and
VM transition/refusal cases, then rehearse the exact native stage with
`tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check` and
`--probe-check --probe-missing-workspace`. Preserve recovery and save the normal
run record before install/one-shot reboot.

**Tell the user this test needs AC unplug/reconnect before triggering reboot.**
Keep the designated USB connected throughout. Start on AC; follow the desktop's
three prompts (connected, unplugged, reconnected). Each phase waits for three
fresh readings, saves a screenshot, then advances. Each prompt has a two-minute
limit; no 15-second photo window is involved. Successful or safely retired
incomplete trials return to Ubuntu automatically. Uncertain live ownership
retains the mount and halts visibly.

Collect the matching new session first. Require `battery_home_transitions` proof,
all four logs and all three verified screenshots, source/release identity,
retirement order, prior-file preservation and offline filesystem/allocation
checks. A missing image or timeout is incomplete evidence. The saved indicator
and AC/current changes establish transition behavior only, not calibrated gauge
accuracy. See the [v52 contract](battery-home-lifecycle.md#guided-home-ac-transitions-v52).
