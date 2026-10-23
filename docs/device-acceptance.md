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
