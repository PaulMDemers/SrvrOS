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
PWM writes, provider binding, EC port accesses or AML execution. Requesting a
brightness provider on this profile still refuses without register access.

Build and stage with `python3 tools/hardware_boot.py stage --laptop-probe-volume 6ADA-85BB`.
Rehearse the exact stage with `tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table gpt`,
then the missing-workspace case. Preserve accepted desktop and console payloads,
verify all prior USB hashes and both offline checks, install with `--keep-previous`,
and save the durable run record before one-shot boot.

After return use `tools/hardware_desktop.py --uuid 6ADA-85BB`, match a new session
and release, and require the `air61-backlight: read-only` record plus completed,
saved and unmounted markers. Check mode, period and duty before designing the
write profile. Verify prior files and offline filesystem/allocation integrity.
VM refusal on Q35 validates recovery, not native register access or panel control.
Visible brightness adjustment needs a subsequent exact-gated provider and attended
trial; no other model's period or mode is accepted as this unit's evidence.
