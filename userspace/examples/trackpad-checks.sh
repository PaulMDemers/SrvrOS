#!/fat/bin/sh
# Attended trackpad-only follow-up; preserve the original device round.
reports="$HOME/trackpad-checks"
if [ -e "$reports" ]; then
    echo 'trackpad-checks: existing reports retained; collect before another run'
    exit 1
fi
mkdir "$reports" || exit 1
echo started > "$reports/sequence.txt" || exit 1
echo 'Trackpad-only check. Reports go to Home/trackpad-checks. No timed prompts.'
echo 'Type each observation on one line; use Text for longer notes.'
echo 'TRACKPAD: enable one- and two-finger taps in Settings. Disable while typing OFF.'
echo 'Over empty desktop, physically press with TWO fingers already resting on pad.'
echo 'A context menu should appear. Escape dismisses it. Lift one finger before'
echo 'releasing the physical click: no primary click should occur. Repeat over a file.'
echo 'Then TAP with two fingers and lift BOTH: a menu should appear without a click.'
echo 'Describe physical secondary press and secondary tap separately.'
read observation || exit 1
echo "secondary-input: $observation" >> "$reports/observations.txt" || exit 1
sync || exit 1
echo 'Drag a titlebar with ONE finger pressed first, then add a steering finger.'
echo 'Lift/replace the moving finger, then release click. Drag should end cleanly.'
echo 'Scroll with two fingers, then lift. No extra click/menu should follow.'
echo 'In Sketch, begin a Line drag and press Escape before releasing; it should'
echo 'cancel the unfinished line. Try another line afterward. Do not unplug USB.'
echo 'Describe primary drag, post-scroll clicks and Escape cancellation separately.'
read observation || exit 1
echo "drag-cancel: $observation" >> "$reports/observations.txt" || exit 1
sync || exit 1
echo 'Save a screenshot with Command-Shift-3. In Text save any detailed notes in Home.'
echo 'Leave the screenshot notification time to finish; press Enter here when done.'
read ready || exit 1
echo completed >> "$reports/sequence.txt" || exit 1
sync || exit 1
echo 'Checks recorded. Completion is NOT a pass; reports and observations need review.'
echo 'Return: Ctrl-Alt-Escape. Wait for USB unmounted=yes, then type reboot.'
echo 'If an error appears, photograph it. There is no timed reboot.'
