# MacBookPro11,3 / A1398

Updated: 2026-10-18. State: **planned; no on-device capture or physical result yet**.

## Ownership and setup

One local agent on this machine's Ubuntu installation; suggested branch
`device/macbookpro11-3/bringup`. Follow the
[Ubuntu setup and reboot guide](../ubuntu-device-agent-setup.md) and fill the
[record template](TEMPLATE.md) as evidence becomes available. Agent assignment,
shared base commit, firmware revision and actual unit configuration are pending.

## Identity to confirm

Late 2013 or Mid 2014 15-inch Retina. Intel plus NVIDIA GT 750M is the planning expectation; verify PCI inventory and firmware-selected display route.

USB workspace UUID, installed/recovery release and reboot state are local and
not yet established. Do not reuse Air7,2's values. All boot, input, filesystem,
audio, telemetry and networking capabilities remain **not tested on this model**.
No sibling-model or VM pass should be copied into this record as physical success.

## Initial work

Preserve the boot framebuffer/GPU route. Inventory GMUX and display-audio dependencies before changing GPU power or routing. Treat Retina layout, input, audio and sensors as separate acceptance items.

Capture the actual Linux inventory, record missing reads and driver bindings,
then validate console/RAM-root/USB workspace before the desktop. Keep unsupported
providers unavailable rather than widening hardware matches without evidence.
The optional AML probe stays out of routine startup.

## Next handoff

Fill in the source commit, branch, inventory reference, capability ledger,
local run-record path, accepted recovery and exact next test before reboot.
Publish changes on this device branch for shared review; coordinate generic
input/filesystem/scheduler/UI work with the integration owner.
