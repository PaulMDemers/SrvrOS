# Attended audio, Ethernet and trackpad checks

Use the explicit guided v58 device-check desktop on the reference Air7,2 with its actual
workspace UUID. This focused device round deliberately leaves battery monitoring
off; preserve the independently accepted ordinary battery desktop as recovery.
Stage with `--desktop-volume <local-uuid> --desktop-device-checks`. The recipe
keeps ECM enabled but explicitly opens Help rather than the network-only script;
legacy boot recipes and device gates remain unchanged. The attached adapter must match its
captured descriptors; VID/PID alone is not sufficient evidence of compatibility.

After the normal build, exact-stage VM, recovery and collection preflights in the
[Ubuntu guide](ubuntu-device-agent-setup.md), announce that this test requires
headphone listening, Ethernet cable changes and gestures before rebooting.
Help opens automatically with the focused checklist. In Terminal run:

```sh
sh /fat/share/examples/device-checks.sh
```

The script creates a fresh `Home/device-checks` directory and refuses replay if
it exists. Collect/archive the prior directory before choosing another run; do
not delete earlier evidence to make a retry appear successful. The script saves
partial observations and phase reports throughout. EOF aborts; prompts have no
deadline. It never reboots or claims an overall pass. Command completion cannot
establish audible channels, speaker isolation or visible context menus.

The sequence covers speakers; wired headphone left/right/stereo and speaker
silence; jack indication, volume, Stop and recovery; DHCP/DNS/HTTP before and after
an Ethernet cable cycle; live System network state; two-finger physical secondary
press and tap; one-finger held drag with a steering finger; no extra click after
scrolling; and Escape cancellation of an unfinished Sketch line. Do not unplug
the USB adapter or storage: cable loss and USB removal have different ownership
and recovery behavior. Transport fault cancellation remains synthetic evidence;
Escape in an app does not accept a hardware transport fault.

Start sound at 10%, select outputs manually in Settings, and report missed
checks explicitly. The fixture plays six seconds of left-only, right-only, then
stereo with silence between sections. The Stop replay is also six seconds; if
missed, replay manually and record the actual outcome. Screenshot with
Command-Shift-3, allow the save to finish, then Ctrl-Alt-Escape, wait for
`USB unmounted=yes` and type `reboot` at the monitor. Photograph a failure.

On Ubuntu return, collect the matching release/session before another trial.
Archive `device-checks`, screenshots and logs. Require successful player results
(except deliberate Stop), actual channel/isolation observations, network status
codes and HTTP response, link-down/recovery and UI observations. `completed` in
`sequence.txt` and `network-trial.txt` means only that prompts finished. A positive
report must identify secondary press versus tap; do not accept both from a single
menu screenshot. Verify all prior files, expected preference changes, offline
fsck and independent allocation, Ubuntu default and consumed one-shot.

The network subscript's optional fourth argument `continue` suppresses its final
UI/return instructions when called from this checklist. Existing standalone
network trials retain their behavior. Host regressions exercise EOF, partial
reports, failed observations, nested cable prompts and replay preservation.
VMs validate drivers and scripted mechanics, never human perception.

## Trackpad-only follow-up and pasted reports

After collecting the first round, run `sh /fat/share/examples/trackpad-checks.sh`
for only secondary press/tap, held-click steering, post-scroll suppression and
Sketch Escape. It uses a new `Home/trackpad-checks` directory and never overwrites
`Home/device-checks`. This follow-up still needs physical gestures and observations;
there is no automatic reboot or prompt deadline.

Terminal now replaces CR/LF clipboard line breaks with spaces when the receiving
app has not enabled bracketed paste. Enter submits the single answer. Apps that
explicitly enable bracketed paste retain their multiline text and markers.
An overlong shell `read` drains to the next newline and fails without setting the
variable, so the tail cannot silently answer a later prompt. Earlier terminal
pastes could consume successive reads; a completed checklist from that behavior
does not establish acceptance of the skipped prompts.

## Secondary tap timing refinement

The October 23 Air7,2 follow-up accepted secondary click/tap functionality but
reported overly strict two-finger synchronization. The revised provisional policy
allows the second finger to arrive within 150 ms and the final finger to lift
within 150 ms of the first lift, with a 300 ms total two-finger tap limit.
Single-finger taps retain the 10–200 ms limit. Both fingers must lift before the
secondary pulse; expired chords never become primary clicks. Movement beyond
64 raw units, scrolling, palms, typing suppression and physical button presses
still disarm the tap. These values are host-tested usability tuning, pending
physical evaluation; no hardware contact timing was measured from the screenshot.

For the next attended trial, try relaxed staggered arrival/lift, then deliberate
scrolls and long resting contacts to check for unwanted menus. Recheck ordinary
one-finger taps and held physical drags. Keep earlier trackpad reports archived;
the checklist refuses an existing report directory and must not overwrite it.

The combined follow-up uses `sh /fat/share/examples/tap-checks.sh` and creates
`Home/tap-checks`, preserving both earlier checklist directories. This is the earlier trial; the focused diagnostic now appears first in Help. After tap observations it pauses for System's live Network view:
unplug only Ethernet, capture the down display, record it while still disconnected,
then reconnect, capture the up display and record again. Each phase saves an
`ifconfig` snapshot. These snapshots do not substitute for the prompted visual
observations or screenshots. No audio replay or timed exit is included.

The native timing follow-up still triggered only about one in three attempts.
The 150/300 ms policy is not accepted as a fix. `trackpad-tap-reject` summaries
now count the first rejection of each armed candidate by rule (movement, total
age, join/lift timing, contact replacement, palm/typing/physical/policy and clock
conditions). They include the number whose peak had reached two fingers and
independent maximum candidate age and displacement. A rejection before the
second finger arrives cannot establish the user's intended gesture. These are
bounded aggregates, not raw contact traces or attempt/success ratios; the next
native run must pair them with deliberate gestures and user observations.
Counters survive transport cancellation and reset with explicit capture reset;
no per-report printing, allocation, or new threshold change is added.

The same trial confirmed live System link-state updates in logs and the user's
notes, although both screenshots were captured while connected. This script
only reads interface state; it does not acquire a DHCP lease. To test Internet
connectivity, run `dhcp` first and inspect its result, then DNS/ping/HTTP. A cable
link alone does not configure an address, router or DNS server.

For the focused rejection capture run `sh /fat/share/examples/tap-diagnostics.sh`.
It creates Home/tap-diagnostics-v2 without changing earlier reports. Move the pointer
once to empty desktop, make 20 natural two-finger taps with full lift and a short
pause after each attempt. Press Escape ONLY to dismiss a visible menu; when no
menu appears, press no key. Then Alt-Tab back to Terminal to record the
menu count. Avoid unrelated gestures. Exit normally so the session's xHCI summary
is saved. Prompt completion and aggregate rejection counts alone are not a pass.

The first diagnostic native sample saved 9 secondary taps and 14 two-finger
movement rejections, with no two-finger timing rejection. The tester saw 8–9 menus
from about 20 attempts. An Escape byte prefixed the saved answer because Escape
on a failed tap could reach the focused Terminal; the script still completed.
Policy `taps-v3` keeps 150 ms join/lift and 300 ms total limits, and raises only
secondary displacement tolerance from 64 to 128 raw Manhattan units (4 to 8 base
pixels). Primary taps retain 64. This is a provisional native-informed adjustment,
not physical acceptance; verify natural taps and unwanted menus during scrolling.
Whole-session rejection totals include ordinary pointing and cannot establish a
precise success ratio or an optimal displacement threshold.

The tester accepted the follow-up tap feel on installed release `95db354d397095b4`
as "dialed in well enough". Further threshold tuning stops here. That returned
round has no saved logs/report: the USB reached 1023 files, exhausting the driver's
then-current 1024-file table after reserving a metadata temporary slot. A disposable host-image
reproduction confirms the boundary; clean fsck does not imply creation headroom.
Keep subjective input acceptance separate from the incomplete session receipt.
PR99 subsequently added verified archival/admission, and PR102 validated demand
allocation beyond the former limits in host/VM and native persistence trials.
See the [Air record](machines/macbookair7-2.md); these fixes do not retroactively
turn the missing tap-trial receipt into a full pass.
