# MacBookAir6,1 brightness bring-up

The first trial is an unattended **read-only PWM capture**, not a brightness
adjustment test. It uses the existing v19 laptop diagnostic profile, saves USB
session logs, unmounts and returns automatically to Ubuntu. Leave the prepared
USB attached; no terminal commands or Settings changes are required. Missing
completion/return or a halt is incomplete/failed evidence.

Admission requires Intel GPU `0a26`, PCH `9c43` and the exact locally captured
31,777-byte DSDT (FNV-1a `af9a47bc352dc6bf`). The fingerprint is an identity
check, not authentication. Only the existing Haswell CPU/PCH PWM register pages
are mapped read-only: BAR0 offsets `48250`, `48254`, `c8250`, `c8254`.
The diagnostic logs raw values and decoded mode/duty/period. It performs no
PWM writes, provider binding, EC port accesses or AML execution. The read-only diagnostic remains separate from the opt-in brightness provider.

Build and stage with `python3 tools/hardware_boot.py stage --laptop-probe-volume 6ADA-85BB`.
Rehearse the exact stage with `tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table gpt`,
then the missing-workspace case. Preserve accepted desktop and console payloads,
verify all prior USB hashes and both offline checks, install with `--keep-previous`,
and save the durable run record before one-shot boot.

After return use `tools/hardware_desktop.py --uuid 6ADA-85BB`, match a new session
and release, and require the `air61-backlight: read-only` record plus the completed
marker and retained exit log. The final saved/unmounted messages occur after
logging stops; record any observed console result separately, then verify host
return and offline integrity. Check mode, period and duty before designing the
write profile. Verify prior files and offline filesystem/allocation integrity.
VM refusal on Q35 validates recovery, not native register access or panel control.
Visible brightness adjustment needs a subsequent exact-gated provider and attended
trial; no other model's period or mode is accepted as this unit's evidence.

The first native capture (`142dd83ded3d2da4`) observed CPU control/duty 0/0,
PCH control `c0000000`, PCH duty `0ad904cc`: mode 2, period 2777, duty 1228.
These are this unit's native observations; visible adjustment remains untested.


## Attended brightness adjustment

The Air6,1 control profile uses the independently captured period 2777, CPU
control 0 and PCH control `c0000000`, behind the same exact hardware/firmware
identity gate. It changes only duty, bounds requests to 5–100%, verifies readback
and disables the provider on mode/period drift or readback failure. Discovery
queries the current level without writing. Brightness is not a saved desktop
preference and is not promised to persist across reboot.

Stage with `python3 tools/hardware_boot.py stage --desktop-volume 6ADA-85BB --desktop-basic --desktop-brightness`.
This selects v56, an unscaled full desktop with brightness only; it leaves
battery, audio, network, sensors, SMC and AML off. Rehearse the exact stage with
`tools/host-env.sh python3 tools/hardware_boot_smoke.py --desktop-check --workspace-table gpt`.
Q35 must leave brightness unavailable; host tests cover positive writes and
refusal, and this native trial supplies visible panel acceptance.

1. Open Settings and scroll to Display brightness. Record its initial percentage
   and confirm that simply booting/opening Settings did not visibly change it.
   If unavailable or an error appears, stop the adjustment test and return normally.
2. Set about 50%, 25%, 75%, then 100%, pausing to confirm visible change and the
   reported level. Test the 5% minimum briefly, then return to 50%. Check that
   typing and trackpad remain responsive throughout.
3. Close/reopen Settings; confirm it reads back the current 50% level. Capture
   Command-Shift-3 with the slider visible. Report flicker, blanking or a level
   that fails to change; do not treat a numeric readback alone as a visual pass.
4. F10, End, Enter returns to console. Require diagnostics saved and USB unmounted,
   then type `reboot`. This trial has no timed exit or automatic reboot.

Collect the matching new session, provider/brightness request logs and screenshot,
compare prior files and run both offline checks. Preserve read-only capture and
accepted desktop/console backups separately from this unaccepted candidate.


Native control trial `59086591e526632b` is physically accepted for visible 5–100%
adjustment: user report plus successful logged readings across that range, clean
desktop exit and offline USB checks. Final logged level was58%, no screenshot
was saved, and this does not establish brightness persistence across reboot.
