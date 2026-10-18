# Built-in trackpad: first pointer trial

The [physical capture gate](trackpad-capture-2026-10-17.md#physical-result--2026-10-17)
verified keyboard and raw trackpad reports on this MacBookAir7,2. This change
connects those reports to the native compositor's existing pointer path. It is
an opt-in trial, not completion of the desktop input milestone.

## Policy and delivery

`kernel/src/drivers/trackpad_pointer.c` converts exactly one active contact into
relative movement. The initial fixed scale is 16 native units per pixel, with
fractional remainders retained and native Y inverted into screen Y. The first
contact establishes a baseline; lifts and contact-count changes discard it.
Per-frame jumps over 2048 native units on either axis rebase without movement.
The raw `origin` word is not treated as a persistent finger ID.

Physical button state controls the left button, independently of movement.
Malformed reports, transport failure and an observed root-port disconnect reset
the baseline and release a held button. After cancellation a held physical button
must first be released before another press is accepted. Failed endpoints remain
stopped; buffers are retained for possible late DMA. This does not implement
hot-plug recovery, timeout detection for a silent device, or lossless recovery
of an interrupted gesture.

`kernel/src/drivers/mouse_input.c` replaces the old single accumulated mouse
state with a 64-event queue, protected by IRQ exclusion on the current single-CPU
input path. Press and release edges retain their order and click positions;
only consecutive motion events with identical buttons coalesce. Signed movement
saturates instead of overflowing. Trackpad button state is separate from the
existing mouse source, so releasing the trackpad cannot release that mouse.
The existing PS/2 and generic USB mice still share their legacy source slot.

On queue overflow, ambiguous queued input is discarded, a neutral cancellation
event is delivered and currently held buttons are suppressed until physical
release. The native compositor drops drag/resize capture on `MOUSE_CANCEL`;
older consumers still see neutral buttons. The event structure and syscall ABI
size are unchanged. Input ownership across arbitrary processes remains future
work; this is not a general desktop event service.

Two or more contacts pause pointer movement; physical clicks still report.
Scrolling, tap-to-click, acceleration, calibrated palm rejection and multi-finger
drag are deliberately not enabled in this trial. Use one finger to press-drag.
The first physical trial received positive user feedback for trackpad movement,
clicking, dragging and resizing; see the result below.

## Trial workflow

```sh
python3 tools/hardware_trackpad.py prepare --uuid 37D8-C5D0 --pointer
sudo python3 tools/hardware_trackpad.py run --run <printed-run-id>
python3 tools/hardware_trackpad.py collect --run <run-id>
```

The versioned profile `ram-usb-v11-trackpad-<volume>-<run>` selects
`srvros.trackpad=pointer`, RAM root and the UUID-selected USB workspace. Normal
boot profiles and the earlier capture-only profile do not inject trackpad input.
The request consumption, hash/CRC checks, durable completion marker and one-shot
GRUB behavior remain the same as the capture workflow.

The request is consumed before the native desktop opens. In the desktop:

1. Move in every direction, lift and re-touch at another location. Check for
   jumps, reversed axes, and whether the speed feels usable.
2. Physically press-drag the title bar with one finger. Release, move away, and
   check that the window stays put. Try the lower-right resize corner too.
3. Type mixed-case text. Ctrl-N opens another study; click between windows and
   check that typing follows focus. These are temporary study shortcuts.
4. Press Escape to return to the capture screen. Complete its one-/two-finger,
   physical-click and `AaZz09` + Enter checks.

The report includes pointer-motion events, button transitions, policy cancellations, queue overflows and
whether the compositor exited successfully. `pointer_trial=EXERCISED` additionally
requires complete capture coverage, some motion, at least two button transitions,
no policy cancellations/queue overflows and a normal desktop exit. Those counters cover the entire
capture session, including the final checks: they do not prove successful window
movement, good pointer feel, latency or successful drag/resize. Those are user
observations. Every test outcome still halts indefinitely for a manual restart.

## Verification

```sh
make test-pointer test-xhci test-trackpad-capture
PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_hardware*.py'
tools/host-env.sh make -j4 all test-core
tools/host-env.sh python3 tools/desktop_compositor_smoke.py --resolution 1440x900
tools/host-env.sh python3 tools/trackpad_capture_smoke.py --pointer
```

Host tests cover fractional movement, sign/orientation, stationary button edges,
press-drag-release, lift/recontact, multitouch transitions, large discontinuities,
cancellation/re-press, queue wrap, overflow, source separation and saturation.
xHCI tests exercise both capture-only and pointer modes, including keyboard and
bulk-transfer coexistence and release on transport failure. The policy and queue
also have ASan/UBSan coverage.

QEMU verifies the downstream desktop with its emulated mouse and keyboard, and
the new launch/exit/report/USB/halt workflow. It does not emulate Apple's raw
trackpad and must report incomplete physical coverage. The separate physical
result below establishes initial desktop interaction acceptance.


## Physical result — 2026-10-17

**Initial pointer/desktop interaction gate accepted.** The user reported that
entering many characters, dragging, resizing, trackpad movement and clicking all
worked great. This supplies the physical interaction evidence that automated
capture counters alone cannot establish.

- Run `0133f0fdf2364d7b`, release `fecfea827320e7c3`.
- Source `e8fbff2c195d96da620c101cbc7b056655bd10ce`, clean when staged.
- Report SHA-256: `b79ae64f8e7cd89be959422e57ff9cd10326faa2be056ce61b12309ef10a7be8`.
- 2,621 valid trackpad frames, 1,905 pointer-motion events and 18 button
  transitions; 27 lift transitions and at most two simultaneous contacts.
- 98 keyboard reports, exact `AaZz09` check, zero dropped characters.
- Zero malformed packets, transport errors, policy cancellations and mouse-queue
  overflows. The desktop exited successfully.
- Host collection verified report identity/checksums, the separate completion
  marker and the host reboot. Status: `CAPTURED`, `pointer_trial=EXERCISED`.

The report covers roughly 629 seconds including user interaction and waiting;
it is not an input-latency measurement or a sustained I/O stress test. The user
confirmed the listed interactions; focus switching between two windows was not
separately reported. Scrolling, palm rejection, the complete input/focus contract,
device recovery and measured latency remain open, so D1/D2 are not complete.

The original [report](../tools/fixtures/apple-05ac-0291-pointer.txt) and
[completion marker](../tools/fixtures/apple-05ac-0291-pointer.done) are retained
as collector regression fixtures. Host manifests/results remain under
`build/trackpad-tests/0133f0fdf2364d7b/`. The installed and staged payloads were
hash-verified against an independent copy under
`~/.local/share/srvros/known-good/fecfea827320e7c3`, with a sibling acceptance
JSON recording scope and evidence. The saved profile contains a consumed trial
request: repeating the test requires preparing a fresh run, not reusing its ID.

Next: extend pointer delivery with explicit wheel events and build a scrollable
native study to exercise two-finger scrolling, then add conservative palm/typing
suppression and cancellation/focus checks before desktop input acceptance.
