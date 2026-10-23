#!/fat/bin/sh
# Interactive gates use the terminal stdin, never the script file.
echo Ethernet cable recovery test
echo Keep the USB adapter and workspace plugged in throughout.
echo Checks and reports run automatically. Cable prompts have no time limit.
echo started > "$HOME/network-trial.txt" || exit 1
rm -f "$HOME/network-after.txt" "$HOME/network-down.txt" || exit 1
echo Waiting up to 30 seconds for the initial Ethernet link...
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30
do
    ifconfig > "$HOME/network-startup.txt" || exit 1
    if grep -q 'role=active-net0 link=up' "$HOME/network-startup.txt"; then
        break
    fi
    sleep 1
done
if grep -q 'role=active-net0 link=up' "$HOME/network-startup.txt"; then
    echo Initial link is up.
else
    echo Initial link is unavailable. The before report will record any failures.
fi
echo Running connected DHCP, DNS and HTTP checks...
sh /fat/share/examples/network-check.sh ${1:-example.com} ${2:-example.com} ${3:-80} > "$HOME/network-before.txt" || exit 1
cat "$HOME/network-before.txt"
echo UNPLUG the Ethernet cable ONLY, then press Enter here.
read cable_gate || exit 1
sleep 5
echo Checking disconnected state...
ifconfig > "$HOME/network-down.txt" || exit 1
sh -c dhcp >> "$HOME/network-down.txt"
echo disconnected-dhcp-status=$? >> "$HOME/network-down.txt"
cat "$HOME/network-down.txt"
echo RECONNECT the Ethernet cable, then press Enter here.
read cable_gate || exit 1
sleep 5
echo Running recovered DHCP, DNS and HTTP checks...
sh /fat/share/examples/network-check.sh ${1:-example.com} ${2:-example.com} ${3:-80} > "$HOME/network-after.txt" || exit 1
cat "$HOME/network-after.txt"
echo completed >> "$HOME/network-trial.txt" || exit 1
sync || exit 1
echo Reports saved in Home: network-before.txt, network-down.txt, network-after.txt.
echo Completion is not a pass: check DHCP, DNS and HTTP status codes and response.
# An enclosing attended checklist supplies its own UI and return instructions.
if [ "${4:-}" != continue ]; then
echo Open System and select Network for the live-display check.
echo Unplug only Ethernet, wait 15 seconds, then reconnect and wait 15 seconds.
echo Leave System open throughout. Note whether both states update automatically.
echo Save a screenshot with Command-Shift-3 if the display stays disconnected.
echo To return: press F10, End, Return. Wait for USB unmounted=yes.
echo Then type reboot and press Enter. There is no timer.
fi
