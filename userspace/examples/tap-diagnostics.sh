#!/fat/bin/sh
# One deliberate gesture sample; aggregate driver diagnostics are saved on exit.
reports="$HOME/tap-diagnostics"
if [ -e "$reports" ]; then
    echo 'tap-diagnostics: existing reports retained; collect before another run'
    exit 1
fi
mkdir "$reports" || exit 1
echo started > "$reports/sequence.txt" || exit 1
sync || exit 1
echo 'TAP DIAGNOSTICS: 20 two-finger taps, no timer or network checks.'
echo 'Move the pointer ONCE onto empty desktop, outside windows and dock.'
echo 'Then avoid pointing, scrolling, physical clicks and Settings changes.'
echo 'Try 20 natural two-finger taps. Lift both fingers fully after each attempt.'
echo 'Count how many open a menu. Press Escape after EVERY attempt, then pause'
echo 'about one second with fingers lifted. Do not press the physical click.'
echo 'After 20 attempts use Alt-Tab until Terminal is focused again.'
echo 'Type how many menus appeared out of 20, plus any unexpected behavior.'
read observation || exit 1
echo "two-finger-sample: $observation" > "$reports/observations.txt" || exit 1
sync || exit 1
echo 'Optional screenshot: Command-Shift-3. Wait for its save to finish.'
echo 'Press Enter here when ready to finish. Do not try more gestures.'
read ready || exit 1
echo completed >> "$reports/sequence.txt" || exit 1
sync || exit 1
echo 'Sample recorded; completion is NOT acceptance. Driver counters save on exit.'
echo 'Ctrl-Alt-Escape, wait for USB unmounted=yes, then type reboot at the monitor.'
echo 'No timed exit. Photograph any failure before restarting.'
