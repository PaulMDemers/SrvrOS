#!/fat/bin/sh
# Explicit attended checks. Never infer perception or overall PASS from completion.
reports="$HOME/device-checks"
mkdir "$reports" || exit 1
echo started > "$reports/sequence.txt" || exit 1
echo 'DEVICE CHECKS: headphones, Ethernet and trackpad. No prompt has a deadline.'
echo 'Reports are saved in Home/device-checks. This folder cannot be reused.'
echo 'Keep both USB devices connected. Start with headphones UNPLUGGED.'
echo 'Open Settings, scroll to Sound, select Speakers and set volume to 10 percent.'
echo 'Return here and press Enter for the left-only, right-only, then stereo sample.'
read ready || exit 1
playwav /fat/share/examples/stereo.wav > "$reports/speakers.txt"
echo player-status=$? >> "$reports/speakers.txt" || exit 1
cat "$reports/speakers.txt"
echo 'Describe speaker channels and clarity, or type skipped/failure. Press Enter.'
read observation || exit 1
echo "speakers: $observation" > "$reports/observations.txt" || exit 1
sync || exit 1
echo 'Plug in headphones. In Settings check the jack label and select Headphones.'
echo 'Keep volume at 10 percent. Speakers must stay silent. Press Enter when ready.'
read ready || exit 1
playwav /fat/share/examples/stereo.wav > "$reports/headphones.txt"
echo player-status=$? >> "$reports/headphones.txt" || exit 1
cat "$reports/headphones.txt"
echo 'Describe left/right/both, jack detection and speaker silence. Press Enter.'
read observation || exit 1
echo "headphones: $observation" >> "$reports/observations.txt" || exit 1
sync || exit 1
echo 'In Settings compare 10 and 25 percent with Test sound. Keep volume comfortable.'
echo 'Then press Enter here to replay; switch to Settings and press Stop during sound.'
read ready || exit 1
playwav /fat/share/examples/stereo.wav > "$reports/headphone-stop.txt"
echo player-status=$? >> "$reports/headphone-stop.txt" || exit 1
echo 'After Stop, try Test sound again. Unplug headphones and select Speakers.'
echo 'Test once more: speakers should recover, jack label should show disconnected.'
echo 'Describe volume, Stop/recovery and unplug behavior, or any missed check.'
read observation || exit 1
echo "audio-controls: $observation" >> "$reports/observations.txt" || exit 1
sync || exit 1
echo 'Next: automatic network reports with two cable prompts. Keep USB attached.'
HOME="$reports" sh /fat/share/examples/network-trial.sh ${1:-example.com} ${2:-example.com} ${3:-80} continue || exit 1
echo 'Open System / Network. Unplug only Ethernet; wait 15 seconds, reconnect, wait.'
echo 'Does the live link display follow both changes without reopening the window?'
echo 'Optionally run DNS example.com in Network Utility. Record results here.'
read observation || exit 1
echo "network-ui: $observation" >> "$reports/observations.txt" || exit 1
sync || exit 1
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
