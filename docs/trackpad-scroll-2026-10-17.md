# Two-finger scrolling and accidental-touch trial

This follows the [accepted pointer trial](trackpad-pointer-2026-10-17.md#physical-result--2026-10-17).
The accepted release remains independently backed up. The new behavior is enabled
only by `srvros.trackpad=gestures`; capture-only and pointer-only modes retain
their prior policies. Initial two-finger scrolling is physically accepted;
accidental-touch protection remains unverified on hardware.

## Input policy

Exactly two contacts, without a physical button press, produce pixel-based wheel
events from their centroid. Summing the coordinates before scaling avoids losing
half-unit movements and makes contact-array reordering irrelevant. Contact-count
changes, lift, cancellation and large coordinate discontinuities rebase without
movement. One finger retains the accepted 16 native units/pixel pointer scale.
Two-finger scrolling has the same scale and natural direction: content follows
the fingers. There is no momentum, tap-to-click or acceleration yet.

For the built-in composite keyboard, ordinary non-modifier key presses suppress
incidental trackpad input for 300 ms and while a repeating key remains held.
Shift is allowed; Ctrl/Alt/Meta shortcuts do not start the typing guard. A touch
that began during suppression stays ignored until lifted. Physical press-drag
is an explicit override of typing suppression, including a drag already underway.

A contact wider than 600 raw touch-major or touch-minor units, or more than two
contacts, latches broad-contact suppression until every contact lifts. A physical
press cannot override it. This is a provisional conservative filter, **not a
calibrated palm classifier**. It can miss small palms or reject broad fingertips;
the first hardware trial must check both false acceptance and false rejection.
The report records suppressed-frame counts and maximum observed touch dimensions
so the threshold can be revised from physical evidence. Suppression releases a
held button and resets movement baselines. A release is required before the next
press after a rejected contact. No claim of complete palm rejection is made.

## Delivery and native study

Mouse syscall 18 now appends signed horizontal/vertical wheel pixels after the
entire original 32-byte userspace structure. The new structure is 40 bytes; old
callers receive only their declared prefix. New callers zero the extension before
calling, so an older kernel produces zero wheel deltas. Existing button/cancel
semantics are preserved. Invalid headers are rejected before dequeue.

The bounded mouse queue keeps scroll separate from pointer motion, retains
button-edge positions, and does not coalesce opposite scroll directions. The
compositor sends server-only `DESK_SCROLL` packets to content under the cursor,
without changing keyboard focus. It ignores wheels during drag/resize. Per-client
bounded queues retry delivery under transport pressure, retain direction changes,
and are discarded on cancellation/client retirement. An unresponsive/full client
is closed using the existing bounded shutdown path rather than retargeting its
pending input to another window. Scroll amounts saturate at 16,384 pixels; the
study's entire document is smaller than that.

The native study has 48 numbered lines, a clipped viewport, a proportional
scroll indicator, and a separate typing area. It clamps at both ends and on
resize. It currently consumes vertical wheel deltas; horizontal deltas are
transported but this study has no horizontal document scrolling. Text is a
rendering/input study, not a saved document or text editor.

## Hardware trial

```sh
python3 tools/hardware_trackpad.py prepare --uuid 37D8-C5D0 --gestures
sudo python3 tools/hardware_trackpad.py run --run <run-id>
python3 tools/hardware_trackpad.py collect --run <run-id>
```

The versioned `ram-usb-v12-trackpad-<volume>-<run>` profile uses the same UUID-gated
USB request, report checksum, completion marker and one-shot Ubuntu return as
previous trials. The screen never disappears on a countdown.

1. Move the pointer over the numbered document and scroll up and down with two
   fingers. Check the direction, small movements, both ends, and lift/re-touch.
2. Recheck one-finger movement, physical clicking, dragging and resizing. Ctrl-N
   opens a second study; scrolling over the other window should not steal the
   keyboard focus from the one you clicked.
3. While typing, lightly brush the pad without clicking. Lift afterward and
   check that movement resumes. Try resting the edge of a palm, then lifting it;
   note unwanted motion or normal fingers being rejected. Avoid pressing down
   during the incidental-touch check, since that deliberately overrides typing
   suppression.
4. Escape returns to live capture checks. Repeat the typing/brush and palm-edge
   checks while watching the TYPING and BROAD counters. Lift fully between checks.
   A zero counter remains NOT YET; do not force a pass by changing the gesture.
   Press Escape to clear trial typing, type `AaZz09`, then Enter. The report saves
   and the system halts for manual restart.

The gesture desktop shows this checklist below the initial window on the target
1440×900 panel. The console repeats the instructions and refreshes cumulative
filter counts, maximum touch dimensions, motion and scroll counts regularly.
It consumes unused pointer events after desktop exit so a long manual
check cannot fill the desktop input queue. Escape clears the text buffer without
saving; Enter retains the explicit save-and-halt behavior. Counters labelled
EXERCISED mean the code path ran, not that physical rejection felt correct.

`gesture_trial=EXERCISED` requires the prior pointer/capture gates and some wheel
events. Typing/palm suppression counters describe frames, not verified palms or
successful accidental-touch rejection. Desktop scrolling, focus behavior and
filter feel still need user observations. A zero suppression count means that
filter was not exercised in that run, not that it passed. Session duration is
not latency; simultaneous sustained storage I/O and recovery remain later gates.

## Verification

Host tests cover centroid motion, direction reversal, contact reordering,
one-/two-finger transitions, lift, fractional units, suppression/press override,
release and recovery. Actual syscall tests protect the old ABI prefix and guard
bytes. Queue, GUI authorization and compositor tests cover direction/order,
content hit testing, unchanged focus, full transport queues and retirement.
The study tests scroll bounds, resize limits, visible image change and buffer
guards. The xHCI tests combine raw keyboard/trackpad/bulk completions and verify
wheel generation. Existing physical capture fixtures continue to validate.

```sh
make test-pointer test-xhci test-desktop test-trackpad-capture
PYTHONPATH=tools python3 -m unittest discover -s tools -p 'test_hardware*.py'
tools/host-env.sh make -j4 all test-core
tools/host-env.sh python3 tools/desktop_compositor_smoke.py --resolution 1440x900
tools/host-env.sh python3 tools/trackpad_capture_smoke.py --gestures
```

The compositor QEMU smoke uses emulated keyboard/mouse for the established
interaction checks. Its explicit `--scroll-smoke` path injects wheel deltas at
the compositor routing boundary and verifies client offsets 120 then 96. This
checks routing, application scrolling and composition, not physical trackpad
transport. The gesture-profile smoke checks launching, exit, persistence, replay
refusal, read-only failure and indefinite halt; absent Apple hardware must still
report incomplete physical coverage. A new laptop trial is required.


## Physical result — 2026-10-17

**Initial two-finger scrolling gate accepted.** The user reported: “The scrolling
with two fingers worked as expected.” This establishes scroll feel on the laptop;
it does not establish palm rejection or the full D2 input milestone.

- Run `c2bd37adbee95638`, release `a39982cf4c4499be`.
- Source `97c45148fb5f3f476d7831bdf920fcee05d2bf70`, clean when staged.
- Report SHA-256: `e55d72fa3be3ae33f1014baff09b4a79e9da206d347056e56757ddda7a85199e`.
- 4,526 valid trackpad reports, 1,325 scroll events, 2,133 pointer-motion events,
  30 button transitions and 58 lifts; at most two simultaneous contacts.
- Exact `AaZz09` keyboard check passed; zero dropped keyboard input, malformed
  packets, transport errors, pointer cancellations or mouse-queue overflows.
- Normal desktop exit; host collection verified identity, both checksums,
  completion marker and host reboot. `gesture_trial=EXERCISED`.
- Both typing and broad-contact suppression counters were **zero**. Neither
  filter was exercised in this capture. Maximum observed touch dimensions were
  343 major / 322 minor raw units, below the provisional 600-unit threshold.

The [original report](../tools/fixtures/apple-05ac-0291-scroll.txt) and
[completion marker](../tools/fixtures/apple-05ac-0291-scroll.done) are retained as
collector regression fixtures. Manifests and collection results remain under
`build/trackpad-tests/c2bd37adbee95638/`. Installed and staged payloads were
hash-verified against the independent backup at
`~/.local/share/srvros/known-good/a39982cf4c4499be`, with a sibling acceptance
JSON that limits acceptance to scrolling. The previous accepted pointer build
is also retained. The consumed test request requires a fresh run to repeat.

The follow-up trial adds explicit desktop instructions and live console filter
counters without changing the gesture policy or thresholds. Its QEMU workflow
also fills the trial typing buffer, clears it with Escape, and verifies the exact
keyboard string in the saved report. Physical filter acceptance is still pending.
Next: verify accidental-touch behavior, then input focus/cancellation and
keyboard/trackpad operation during filesystem activity.
No physical latency or sustained-I/O claim follows from this short trial.
