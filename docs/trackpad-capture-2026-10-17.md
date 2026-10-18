# Physical trackpad capture workflow

This is the next hardware gate after the
[native compositor checkpoint](desktop-compositor-2026-10-17.md). It captures the
MacBookAir7,2's built-in keyboard/trackpad together and persists the evidence to
the existing USB workspace. It does not implement pointer motion or gestures.

## What the user does

The dedicated boot shows instructions and live counters. There is no countdown.

1. Swipe with one finger, then lift it.
2. Swipe with two fingers, then lift them.
3. Physically press the trackpad to click several times; tapping alone is not
   the click test.
4. On the built-in keyboard, type exactly `AaZz09` and press Enter. Backspace
   corrects the input before submitting it.

The guest writes its report, explicitly flushes and verifies it, writes and
flushes a separate completion marker, and attempts to unmount the USB workspace.
It then **halts indefinitely**, on success or failure. Photograph the screen if
anything is incomplete or failed, then restart manually. If the keyboard stops
working, the live screen remains available for a photograph and manual restart.
Ubuntu remains the default after the one-shot boot.

## Prepare, launch, collect

The USB volume must already contain the valid workspace marker. Preparation
preserves existing workspace files and creates only a uniquely named request.

```sh
tools/host-env.sh make -j4 all test-core
PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_hardware*.py'
tools/host-env.sh python3 tools/trackpad_capture_smoke.py
python3 tools/hardware_trackpad.py prepare --uuid 37D8-C5D0
sudo python3 tools/hardware_trackpad.py run --run <printed-run-id>
```

`run` verifies the staged hashes/profile, exact unconsumed request and Ubuntu
default. It installs the selected release, retains the prior installed release
as `srvros-previous`, unmounts the host USB volume, arms `srvros-current` once and
requests the reboot. The independent known-good backups remain under
`~/.local/share/srvros/known-good/`.

After restarting into Ubuntu and mounting the flash drive:

```sh
python3 tools/hardware_trackpad.py collect --run <run-id>
```

The files are `srvros-work/trackpad-<run>.req`, `.txt` and `.done` on the USB.
Host manifests, collected reports and validation results are under
`build/trackpad-tests/<run>/`. The collector checks identity, size bounds, inner
report CRC, the separately flushed completion marker's CRC, coverage consistency
and a changed Ubuntu boot ID. A report file alone is not completion evidence.
An incomplete but valid capture is still copied for diagnosis and returns status
2. A missing/invalid completion marker is rejected.

## Evidence collected

The driver retains bounded first-frame, first-touch, first-multiple-touch,
first-button and first-malformed packet samples before rearming the DMA buffer.
The report stores those bytes as hex, plus actual lengths, completion code,
contact/button frame counts, maximum simultaneous contacts, lift transitions,
malformed/transport errors and keyboard report/drop counts. A snapshot copies
the counters and samples with interrupts masked so they describe one consistent
point in time. Per-session capture resets do not hide earlier transport failures.

`CAPTURED` requires a ready device, touch, two simultaneous contacts, physical
button frames, a lift, no malformed/failed transfers, exact mixed-case text and
new reports from that same device's keyboard endpoint with no dropped characters.
Anything less is `INCOMPLETE`. These labels describe evidence coverage, not
acceptance of scrolling, palm rejection, input latency or the desktop experience.

The profile is `ram-usb-v10-trackpad-<volume>-<run>` and selects RAM root,
UUID-gated workspace writes, opt-in endpoint capture, run ID and release ID.
It does not enable generic USB writes or internal AHCI storage. The request is
consumed and flushed before collecting input; a stale profile cannot overwrite
an existing report or repeat an already consumed run.

## Regression coverage

The driver tests exercise real descriptor parsing and synthetic completion
events, including retained bytes after DMA reuse, two contacts, lift, malformed
reports, reset, and snapshot behavior. Host report tests exercise option parsing,
maximum packet sizes, coverage and CRC generation; the Python collector consumes
the actual C-generated report and rejects corruption, truncation, missing
completion, identity mismatches and inconsistent coverage.

The QEMU integration uses disposable exfatprogs-formatted USB media. It tests
interactive input, report/marker persistence, independent clean-filesystem checks,
consumed-request replay with unchanged disk bytes, read-only write failure, and
halting. QEMU has no matching Apple device and must report `INCOMPLETE`; that is
the expected result, not evidence of hardware support. Logs are under
`build/review/trackpad-*`.
