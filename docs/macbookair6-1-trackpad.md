# MacBookAir6,1 trackpad trials

This is an opt-in trackpad bootstrap for the locally captured Apple
`05ac:0290`, revision `0171`. Bounded native pointer/drag/scroll acceptance is recorded in the
[unit record](machines/macbookair6-1.md); remaining input filters are not fully accepted.

The October 22 Linux inventory supplies an 84-byte USB configuration identical
to the captured `0291` configuration: keyboard interface 1/EP82 and trackpad
interface 2/EP83, full-speed, 64-byte packet, interval 1 for the trackpad.
Linux v6.12's [bcm5974 source](https://github.com/torvalds/linux/blob/v6.12/drivers/input/mouse/bcm5974.c)
identifies 0290 as Wellspring8 ANSI, uses Type 3 and requires no mode switch.
The existing bounded Type 3 decoder can therefore be exercised without a new
feature-control write. Native packet evidence was collected in run `77ab86681e27132d`; see the
[unit acceptance record](machines/macbookair6-1.md). A later v12 run exercised pointer/drag/scroll behavior with positive user
feedback; calibration and sustained-use coverage remain limited.

Admission requires this PID/revision and explicit capture, pointer or gesture
mode. Other revisions and mismatched endpoint descriptors refuse. Raw capture and a bounded pointer/gesture trial are accepted; new trials
still require their own staged payload and evidence. Existing
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

The exact rehearsal verifies the staged hashes and v10/v11/v12 recipe, uses its
unmodified loader/kernel/initramfs/config through GRUB, and attaches disposable
GPT USB media with that stage's UUID/request. It verifies report/completion CRCs,
clean unmount and offline allocation, consumed-request replay refusal, read-only
failure refusal, and halt behavior. QEMU has no matching Apple trackpad, so its
valid saved report must say `INCOMPLETE`. The PASS receipt under
`build/review/trackpad-stage-<release>/trackpad-result.json` binds the release and
manifest hash. This never reads or writes the physical USB during VM execution.
Without `--release`, the existing generic MBR/ISO workflow remains available.
Staged mode infers capture (v10), pointer (v11), or gestures (v12) from the
verified recipe. Desktop I/O profiles remain unsupported in exact-stage mode.

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


## Desktop pointer and scrolling trial

After building and passing host tests, use
`python3 tools/hardware_trackpad.py prepare --gestures --uuid <observed-uuid>`.
Rehearse the printed release with the same `--release` / `--workspace-table gpt`
command above; do not add `--gestures` to the exact-stage harness command.
Install with `--keep-previous` to preserve the accepted console/USB recovery.
The VM confirms desktop startup/exit and persistence, but has no Apple reports:
its pointer/gesture coverage must remain `INCOMPLETE`.

The v12 test starts a small desktop trial. Move one finger in all directions;
check slow precise movement and longer swipes. Physically click the window,
then hold a one-finger click on its title bar and move to drag it. Release and
check the drag ends. Try holding that click while adding a second finger to
steer, then lift/reposition the moving finger; report jumps or stuck drags.
With the pointer over the window content, use two fingers without pressing to
scroll both directions. Note whether content direction and speed feel right.
Type a few characters to verify keyboard coexistence.

Press Escape to return to capture/filter checks. Follow the on-screen hold-A
and broad-contact prompts; broad-contact detection is provisional and must not
be forced to pass. Lift fully between checks, press Escape to clear the typing,
then type `AaZz09` and Enter. Require saved report and successful USB unmount.
The test HALTS: manually restart into Ubuntu. Bring back observations about
movement direction/speed, click/drag/release and scrolling; counters alone do
not establish usable behavior. Do not rerun the consumed request.

## Tap-to-click and typing suppression trial (v50)

The v12 study leaves pointer options at zero; compositor cleanup also resets
options. Its zero tap/typing counters therefore do not demonstrate hardware
failure. Use the separate v50 profile with
`python3 tools/hardware_trackpad.py prepare --tap-typing --uuid <observed-uuid>`.
Rehearse the printed release using `trackpad_capture_smoke.py --release <release>
--workspace-table gpt`, then install with `--keep-previous`. v50 enables primary
and secondary taps plus disable-while-typing, at speed 3 without acceleration,
only for this attended study. It restores these options for console checks
after compositor exit and clears them before saving. Ordinary preferences and
old profile recipes are unchanged. v50 cannot combine with desktop I/O.

In the desktop, lightly tap **Text size** with one finger without pressing the
physical click. It should activate once per short tap. Try several deliberate
taps with a full lift between them. Hold A while brushing/moving a finger on
the pad without pressing: the pointer should stay still. Release A AND lift
fully, then touch again: movement should resume. Repeat taps after typing;
physical click/drag and scrolling must not produce an extra tap on release.
Long holds and movement should not tap. Secondary two-finger taps are optional
in this study; record their behavior without assuming a context menu is present.

Escape returns to live tap/typing counters. Require primary taps and typing
suppression frames to increase; if they do not, report incomplete coverage.
Follow the remaining physical click/scroll/keyboard prompts, Escape to clear,
then `AaZz09` and Enter. Wait for saved report and successful unmount, then
manually restart after HALT. The collector's `tap_typing_trial=EXERCISED` requires
previous gesture coverage, primary taps, typing suppression and configured
options. It does not prove click accuracy or recovery feel; report those.
