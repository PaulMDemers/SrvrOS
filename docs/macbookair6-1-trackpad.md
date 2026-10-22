# MacBookAir6,1 trackpad capture

This is an opt-in raw-report bootstrap for the locally captured Apple
`05ac:0290`, revision `0171`. It is not pointer/gesture acceptance.

The October 22 Linux inventory supplies an 84-byte USB configuration identical
to the captured `0291` configuration: keyboard interface 1/EP82 and trackpad
interface 2/EP83, full-speed, 64-byte packet, interval 1 for the trackpad.
Linux v6.12's [bcm5974 source](https://github.com/torvalds/linux/blob/v6.12/drivers/input/mouse/bcm5974.c)
identifies 0290 as Wellspring8 ANSI, uses Type 3 and requires no mode switch.
The existing bounded Type 3 decoder can therefore be exercised without a new
feature-control write. Native packet evidence is still required.

Admission requires this PID/revision and explicit capture-only mode. Pointer,
gestures, other revisions, and mismatched endpoint descriptors refuse. Existing
0291/0262 behavior remains covered by host regression tests. Synthetic completions
exercise the new PID's reports, keyboard/bulk coexistence, stale events and errors;
no synthetic result establishes physical coverage or coordinate calibration.

## Prepare and rehearse

Follow the [Ubuntu guide](ubuntu-device-agent-setup.md) and this unit's
[record](machines/macbookair6-1.md). Use its actual exFAT UUID, never an example
from another machine. Keep accepted console release `c35475f62f154f1e` as recovery.

```sh
tools/host-env.sh make -j2 all usb-image test-core
PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_hardware*.py'
python3 tools/hardware_trackpad.py prepare --uuid <observed-uuid>
# Read the printed release; do not stage anything else during this rehearsal.
python3 tools/trackpad_capture_smoke.py --release <printed-release> --workspace-table gpt
```

The exact capture rehearsal verifies the staged hashes and v10 recipe, uses its
unmodified loader/kernel/initramfs/config through GRUB, and attaches disposable
GPT USB media with that stage's UUID/request. It verifies report/completion CRCs,
clean unmount and offline allocation, consumed-request replay refusal, read-only
failure refusal, and halt behavior. QEMU has no matching Apple trackpad, so its
valid saved report must say `INCOMPLETE`. The PASS receipt under
`build/review/trackpad-stage-<release>/trackpad-result.json` binds the release and
manifest hash. This never reads or writes the physical USB during VM execution.
Without `--release`, the existing generic MBR/ISO workflow remains available.
Staged mode deliberately accepts capture-only v10, not pointer/desktop recipes.

Install only after that receipt passes. Preserve the accepted previous release
on later installs with `--keep-previous`; retain independent payload/evidence
backups outside `build`. Preparation and installation do not authorize a reboot.
The generic `hardware_trackpad.py run` combines installation and reboot; after
separate preparation use the recorded one-shot workflow and save `launch.json`
with the pre-reboot boot ID so the collector can correlate the return.

## Attended test

The on-screen tester needs no monitor commands:

1. Swipe one finger, then lift it.
2. Swipe two fingers together, then lift both.
3. Physically press the trackpad several times (taps do not test the button).
4. Type exactly `AaZz09` and press Enter. Backspace corrects typing.

It saves the bounded report and separate durable completion marker, attempts
USB unmount, then **halts**. It does not return automatically. Read the result
and `USB unmounted` message before manually restarting into Ubuntu; photograph
failure output. A missing marker, failed save/unmount or incomplete coverage is
not a pass. If the keyboard stops working, photograph the screen for diagnosis.

After Ubuntu return, collect the recorded run with
`python3 tools/hardware_trackpad.py collect --run <recorded-run>` before staging
anything else. Preserve report and marker, compare pre-test file hashes (the
request is intentionally consumed), then run offline exFAT/allocation checks.
Use the captured packets to decide the next pointer trial; do not enable normal
desktop input based only on this VM or configuration-descriptor similarity.
