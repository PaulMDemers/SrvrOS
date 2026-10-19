# Trackpad interactions for the MacBook Air

Status: physical held-click two-finger dragging accepted by the user on hardware.
One-finger primary and two-finger secondary taps implemented; physical tap
acceptance pending. Tap dragging and three-finger gestures remain planned.
The v22 physical round recorded five primary taps and one secondary tap, but
the user reported no visible right-click result when physically pressing with
two fingers. Physical secondary press is not yet implemented; it still produces
primary click. Adding it while preserving held-click steering is the next input
task. See the [machine record](machines/macbookair7-2.md).
Target: MacBookAir7,2 built-in Apple 05ac:0291 Type 3 trackpad. This is a plan for
SrvrOS behavior, not a claim of macOS compatibility or physical acceptance.

## User-facing contract

Keep input preferences together in Settings → Trackpad. Disable while typing is
off by default. A user can type and move the pointer together without finding an
accessibility panel or running a command. Existing saved settings migrate with
that choice off. Palm rejection remains independent.

The next priority is the user's familiar interaction: one finger holds the click
or selection while another moves the pointer. Cover both a physical held click
and a tap-then-hold gesture. A simple stationary touch without an armed click
must not silently turn a two-finger scroll into a drag.

## Current implementation and gaps

- `apple_trackpad.c` decodes up to 16 contacts with position, size and an `origin`
  field. We have not established that `origin` is a stable contact identifier.
- `trackpad_pointer.c` follows the first contact for one-finger movement. Two
  unpressed contacts scroll using their summed position. Physical held-click
  dragging now tracks two logical contacts using bounded position matching and
  selects the moving contact with hysteresis. Raw report order and `origin` do
  not define identity. Ambiguous matches pause motion while preserving the press.
- Contact-count changes and steering handoffs reset motion baselines. The tracker
  is deliberately limited to two contacts. The tap recognizer emits a primary or
  secondary click only after full lift; there is no tap-drag state yet. Physical button ownership remains independent of contact ownership.
- Three or more contacts currently latch palm suppression. Merely enabling a
  three-finger action would conflict with that rule.
- The mouse queue preserves button edges and cancels on overflow. The compositor
  captures a press to one client and cancels on focus/resize. New gestures should
  reuse that path rather than invent app-specific drag handling.
- Typing suppression was unconditional in gesture mode. The new saved setting
  gates it. With it on, the existing paired internal-keyboard policy retains its
  300 ms interval, held-key extension, lift-to-rearm rule and deliberate physical
  press override. With it off, the next report clears only the typing latch and
  rebases motion; external mice and broad-contact rejection are unaffected.

## Delivery order

### 1. Contact tracking and held-click dragging

The first implementation uses a small two-contact tracker within pointer policy.
Its selection/handoff thresholds are provisional 32/64 raw units (2/4 pixels at
base gain), with a 2048-unit continuity bound. The user accepted held-click
steering on hardware; individual handoff counters remain limited evidence.
Saved exit diagnostics include held-drag frames, rebases and handoffs; rebases
include the deliberate waiting-for-motion phase, not only ambiguous matches.
For later tap and thumb recognition, extend the tracker between decoder and policy. Match
contacts using bounded distance and continuity, retaining age, motion history,
size and an internal ID. Treat ambiguous matches as a reason to rebase, never
as a new click. Validate the report's `origin` field with real traces before
using it as an identity hint; array position alone is not identity.

Give a physical press one owner until physical release or cancellation. Adding a
second finger must not release/re-press the button. A stationary anchor can hold
the click while the moving contact steers. Select the moving contact with a
minimum displacement and hysteresis, rather than alternating every report or
averaging with the stationary finger and halving its travel. If that contact
lifts, pause and rebase onto the remaining or newly placed moving finger.

Pressed movement takes precedence over scrolling. Releasing a press while two
contacts remain must rebase and require deliberate new scroll movement. It must
not deliver accumulated drag displacement as a scroll event. Apply the existing
acceleration curve to the selected moving finger, with fresh timestamps after
handoff. Keep button state stable across contact changes and always release it
on cancellation, disconnect or malformed input.

Acceptance: drag windows, text selections, sliders and scrollbar thumbs with a
stationary thumb plus moving finger. Swap report order, add/lift the steering
finger repeatedly, and release either contact first. No jump, unintended scroll,
new click, app closure or stuck drag is acceptable.

### 2. Tap-to-click and tap-and-drag

Implemented basic taps: one finger produces a primary click, two a secondary
click. Settings has separate saved switches, initially on, including migrated
v1/v2/v3 preferences. v4 stores both choices; desktop protocol v10 carries them.
The current provisional limits are 10–200 ms total contact, 80 ms to form a
chord, 80 ms between first and final lift, and 64 raw units of displacement.
Quiet two-finger candidates do not emit scrolling. Exceeding motion/time bounds,
physical clicking, broad contacts, transport cancellation, clock rollback or a
mid-contact tap-option change invalidates the candidate until all contacts lift.
A completed tap queues adjacent press/release edges at one pointer position,
without retaining synthetic button state or relying on a future report/timer.
The queue refuses taps while another physical button is held and preserves its
existing overflow cancellation. Hardware acceptance and threshold tuning remain
pending. The following tap-drag work has not been implemented.

Recognize a short one-finger down/up with bounded movement as a click. Emit one
press/release pair at a fixed cursor position. A physical click consumes the tap
candidate so it cannot produce a duplicate click on lift. Scrolling, broad
contacts and cancellation invalidate tap candidates.

A completed tap followed promptly by another touch-and-hold arms tap dragging.
Once armed, preserve the logical press while either the original finger moves
or an added second finger steers with the first held still. This is separate
from a single ordinary two-finger scroll. Start with drag lock off: lifting all
fingers ends the drag. Add optional timed/sticky drag lock only after the basic
ownership and release rules pass replay and hardware checks.

Keep timing and distance thresholds in one policy structure with deterministic
monotonic-time tests. Derive displacement thresholds from the captured device
range; do not tune raw constants by guessing from another model of trackpad.
Proposed defaults after physical acceptance: tap-to-click on, tap dragging on,
drag lock off, each with a visible setting.

### 3. Two-finger secondary click and resting-thumb handling

The two-finger tap portion is implemented with the shared tap recognizer above.
It sends the existing right-button input to the desktop context-menu path;
Files, editable fields and the desktop already support contextual actions.
Physical secondary clicks and resting-thumb classification remain future work.

A short, low-movement two-finger tap invokes the existing secondary-click/context
menu path. A moving pair remains scrolling. Decide the gesture before emitting a
button event; do not emit a primary click and then change it into a secondary one.
Physical two-finger click needs the same arbitration: a stationary anchor already
holding a drag is not a request to change buttons. A detected resting thumb must
not count as a second tapping/scrolling finger. Proposed secondary-tap default:
on after physical acceptance.

### 4. Optional three-finger dragging and later gestures

Replace blanket contact-count palm rejection with per-contact size, origin and
motion evidence before enabling three-finger gestures. Three-finger drag should
be an explicit option, initially off. Define precedence against three-finger
workspace swipes; do not bind the same gesture to both.

After reliable click/drag/scroll arbitration: consider scroll inertia, pinch to
zoom in Preview/Sketch, and three/four-finger workspace navigation when those
shell features exist. Pinch should produce a dedicated gesture event for apps,
not synthetic keyboard shortcuts. Do not assume this laptop has Force Touch or
requires a pressure click to support any of the above.

## State and diagnostics

Separate physical contacts, click ownership, motion ownership and gesture mode.
Suggested modes: idle, pointing, tap candidate, physical drag, tap drag,
scrolling, suppressed and canceled. Track physical and synthetic buttons
separately, combining them before the existing mouse queue. Contact changes
alone must not synthesize button edges. Preserve queue cancellation and the
existing external-mouse ownership behavior.

Add bounded trace records for contact transitions, gesture mode, chosen moving
contact, button edges, suppression reason and policy revision. Reuse USB session
logging and raw frame capture; include capture gaps/overflow counters. Typing
traces need activity/timing, not character contents. Keep trace allocation and
filesystem writes out of the report-processing path.

## Verification and hardware rounds

1. Host replay: stable/reordered contacts, crossing or ambiguous matches,
   anchored two-finger drags, repeated finger replacement, click release order,
   rapid tap/scroll changes, broad contact, malformed frames and queue overflow.
2. Policy tests: suppression off allows movement while typing; on suppresses
   incidental touches; disabling mid-contact rebases safely; defaults and v1/v2
   migration remain off; changing it does not alter mouse input or palm handling.
3. QEMU: Settings toggle, verified save, fresh-session reload, disabling again,
   and mouse-button delivery through window/widget capture. QEMU's generic mouse
   cannot prove physical Apple multi-contact recognition.
4. One guided MacBook round per gesture milestone, with persistent on-screen
   instructions and saved traces: repeat each gesture in Text, Files, Preview,
   titlebars and scrollbars, both idle and during USB I/O. Start the round with
   typing suppression off; opt in briefly to check the switch, then leave it off.
5. Acceptance records distinguish automated replay/VM results from the user's
   hardware observations. Keep experimental gestures behind options until their
   physical traces and results are accepted.

## Reference behavior

[libinput tapping documentation](https://wayland.freedesktop.org/libinput/doc/latest/tapping.html)
describes tap-and-drag, configurable drag lock, and steering with a second finger
while the first stays still. These are useful behavioral references; SrvrOS will
implement its own bounded recognizer and replay tests.

[libinput palm-detection documentation](https://wayland.freedesktop.org/libinput/doc/latest/palm-detection.html)
separates touch-size rejection from optional typing suppression and discusses
keyboard pairing, modifier keys, and physical buttons.

[Apple's three-finger-drag instructions](https://support.apple.com/en-us/102341)
show a selectable dragging style. They do not establish support or thresholds
for this particular SrvrOS target; our implementation must use captured hardware
evidence. We will place related controls together in Trackpad settings.
