# MacBookAir6,1 full desktop with USB file work

This is an attended native acceptance trial, separate from the Type study.
v53 starts the full desktop, Home at `/work/srvros-work`, gesture input and
session logging. Optional laptop/AML, audio, sensor and network services stay off.
Ubuntu remains the default and the console recovery payload is retained.

## Session checklist

1. Open Terminal using Command-Space, type `terminal new app`, Enter.
   Run the `sh session-<run>.sh` command recorded in the local run manifest.
   This is a real shell with a `$` prompt, not the `srv>` monitor. The job prints
   60 rounds and takes at least two minutes. Keep that Terminal open.
2. While the counter advances, open Text (Command-Space, `Text new app`, Enter).
   Type a few paragraphs, select text, scroll, move/resize the window and switch
   focus. Use taps and physical clicks. Save with Ctrl-S as the fresh trial's
   `notes-<run>.txt` in Home, reopen it and add/save a second line.
3. Open Files and browse Home while the job continues. Try window dragging and
   scrolling during copies. Check that keyboard input is neither lost nor stuck.
   Change pointer speed in Settings, wait for its save, close/reopen Settings and
   confirm the value. The ordinary preference defaults apply: tap-to-click on,
   Disable while typing off unless explicitly selected. Test suppression only
   after selecting that checkbox; this trial does not force the v50 policy.
4. Wait for `USB SESSION COMPLETE`. An error, a stopped counter or an absent
   completion is incomplete/failed evidence. Do not close Terminal during the job.
   Save Text, take a screenshot with Command-Shift-3, then use F10, End, Enter
   (the desktop menu's Return to console action).
5. Confirm `diagnostics saved=yes` and `desktop returned=yes USB unmounted=yes`.
   Type `reboot` at `srv>` to return to Ubuntu. Photograph any failure instead
   of calling it a pass. No automatic reboot or timed desktop exit is used.

The workload makes a fresh run directory and refuses repeat invocation. It copies
and synchronizes 1 MiB, renames it, compares the readback and records the round,
60 times with two-second pacing. It preserves the final payload, all round numbers
and a completion record. File work runs in Terminal's process tree while the
other apps run; this is not a storage benchmark or measured input-latency claim.

## Preparation and collection contract

Build the matched kernel/userspace, then stage with
`python3 tools/hardware_boot.py stage --desktop-volume 6ADA-85BB --desktop-basic`.
Run `tools/host-env.sh python3 tools/hardware_boot_smoke.py --desktop-check --workspace-table gpt`
on that exact stage. The VM uses a disposable USB volume and the same generated
workload; it checks full app saves during the job, exact retained bytes, logs,
clean filesystem/unmount and preference persistence across a fresh VM boot.

Generate a fresh 16-hex run with `tools/desktop_session_workload.py`'s `files(run)`
and save its two inputs exclusively in this unit's prepared USB Home. Record all
prior file hashes/session names, payload hashes, installed recovery, host boot ID,
VM receipt and checklist outside `build` as well as the local run directory.
Run both offline fsck and allocation traversal before installation. Install with
`--keep-previous`; leave one-shot empty until the user is ready.

After return collect with `python3 tools/hardware_desktop.py --uuid 6ADA-85BB`.
Match the new session's release to the run manifest and require its exit snapshot
and clean desktop exit. Verify the workload with `desktop_session_workload.verify`,
check the saved note/screenshot and preferences, compare all prior file hashes
(allow only documented metadata/preferences/session changes), then run both
offline filesystem checks. A completion file alone does not accept the user's
visible input experience, clean desktop exit or filesystem preservation.
