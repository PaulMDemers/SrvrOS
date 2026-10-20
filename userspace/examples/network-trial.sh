#!/fat/bin/sh
# Interactive gates use the terminal stdin, never the script file.
echo Ethernet cable recovery test
echo Keep the USB adapter and workspace plugged in throughout.
echo Checks and reports run automatically. Cable prompts have no time limit.
echo started > "$HOME/network-trial.txt" || exit 1
rm -f "$HOME/network-after.txt" "$HOME/network-down.txt" || exit 1
echo Running connected DHCP, DNS and HTTP checks...
sh /fat/share/examples/network-check.sh "$@" > "$HOME/network-before.txt" || exit 1
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
sh /fat/share/examples/network-check.sh "$@" > "$HOME/network-after.txt" || exit 1
cat "$HOME/network-after.txt"
echo completed >> "$HOME/network-trial.txt" || exit 1
sync
echo Reports saved in Home: network-before.txt, network-down.txt, network-after.txt.
echo Completion is not a pass: check DHCP, DNS and HTTP status codes and response.
echo You can now inspect System Network and Network Utility.
echo To return: press F10, End, Return. Wait for USB unmounted=yes.
echo Then type reboot and press Enter. There is no timer.
