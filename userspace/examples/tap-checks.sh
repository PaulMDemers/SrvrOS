#!/fat/bin/sh
# Attended timing and live-link follow-up; retain all earlier device reports.
reports="$HOME/tap-checks"
if [ -e "$reports" ]; then
    echo 'tap-checks: existing reports retained; collect before another run'
    exit 1
fi
mkdir "$reports" || exit 1
echo started > "$reports/sequence.txt" || exit 1
echo 'TAP TIMING AND LIVE NETWORK CHECK. No timed prompts. Keep USB attached.'
echo 'Reports go to Home/tap-checks. Type each observation on one line.'
echo 'In Settings enable both tap options; Disable while typing OFF.'
echo 'Over empty desktop, tap with two fingers several times without pressing.'
echo 'Try natural slightly staggered contact and lift; both must lift for a menu.'
echo 'Escape dismisses the menu. Does it feel easier than the previous build?'
read observation || exit 1
echo "tap-timing: $observation" > "$reports/observations.txt" || exit 1
sync || exit 1
echo 'Scroll then lift; rest two fingers for a second then lift: no extra menu.'
echo 'Check one-finger taps and physical two-finger right-clicks.'
echo 'Hold a primary click first, add a steering finger, drag and release.'
echo 'Describe unwanted menus, ordinary taps and dragging separately.'
read observation || exit 1
echo "input-regressions: $observation" >> "$reports/observations.txt" || exit 1
sync || exit 1
ifconfig > "$reports/network-before.txt" || exit 1
echo 'Open System / Network, leave it unpaused and visible beside Terminal.'
echo 'UNPLUG ONLY THE ETHERNET CABLE. Leave the USB adapter and drive connected.'
echo 'Wait about 15 seconds. Does the live link show down without reopening?'
echo 'Capture System with Command-Shift-3; wait for the save to finish.'
echo 'Leave the cable unplugged. Type the observed link state here, then Enter.'
read observation || exit 1
echo "network-down-ui: $observation" >> "$reports/observations.txt" || exit 1
ifconfig > "$reports/network-down.txt" || exit 1
sync || exit 1
echo 'Reconnect the Ethernet cable. Wait about 15 seconds with System visible.'
echo 'Does the live link return to up without reopening? Capture another screenshot.'
echo 'Wait for the save to finish, then type the observed state here and Enter.'
read observation || exit 1
echo "network-up-ui: $observation" >> "$reports/observations.txt" || exit 1
ifconfig > "$reports/network-after.txt" || exit 1
sync || exit 1
echo 'Save any final notes/screenshots. Press Enter when saves have finished.'
read ready || exit 1
echo completed >> "$reports/sequence.txt" || exit 1
sync || exit 1
echo 'Checks recorded. Completion is NOT a pass; observations need review.'
echo 'Return: Ctrl-Alt-Escape. Wait for USB unmounted=yes, then type reboot.'
echo 'If an error appears, photograph it. There is no timed reboot.'
