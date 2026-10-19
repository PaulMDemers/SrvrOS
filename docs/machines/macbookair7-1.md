# MacBookAir7,1 / A1465

Updated: 2026-10-18. State: **planned; no on-device capture or physical result yet**.

## Ownership and setup

One local agent on this machine's Ubuntu installation; suggested branch
`device/macbookair7-1/bringup`. Follow the
[Ubuntu setup and reboot guide](../ubuntu-device-agent-setup.md) and fill the
[record template](TEMPLATE.md) as evidence becomes available. Agent assignment,
shared base commit, firmware revision and actual unit configuration are pending.

## Identity to confirm

Early 2015 11-inch Air. Expect 1366x768 and Intel HD Graphics 6000; verify the actual configuration.

USB workspace UUID, installed/recovery release and reboot state are local and
not yet established. Do not reuse Air7,2's values. All boot, input, filesystem,
audio, telemetry and networking capabilities remain **not tested on this model**.
No sibling-model or VM pass should be copied into this record as physical success.

## Initial work

Compare topcase USB/HID descriptors and firmware with Air7,2; test all layouts at 1366x768; verify audio/backlight/SMC resources independently.

Capture the actual Linux inventory, record missing reads and driver bindings,
then validate console/RAM-root/USB workspace before the desktop. Keep unsupported
providers unavailable rather than widening hardware matches without evidence.
The optional AML probe stays out of routine startup.

## Next handoff

Fill in the source commit, branch, inventory reference, capability ledger,
local run-record path, accepted recovery and exact next test before reboot.
Publish changes on this device branch for shared review; coordinate generic
input/filesystem/scheduler/UI work with the integration owner.
