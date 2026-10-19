# MODEL — device development record

Updated: DATE. State: planned / inventory captured / VM-tested / physical partial
acceptance / accepted for stated scope / blocked by recorded failure.

## Identity and ownership

- Model identifier, enclosure, year/configuration, local unit label:
- Device branch and shared base commit:
- Agent task/subsystem ownership and coordination notes:
- Ubuntu version/kernel, UEFI/GRUB arrangement, firmware revision:
- CPU/RAM, GPU(s)/display, storage controller(s), original/replaced disks:
- PCI/USB/subsystem IDs, audio codecs, topcase report layout:
- Inventory capture reference and missing/partial reads:

## Capability ledger

| Capability | State | Evidence/release | Remaining work |
| --- | --- | --- | --- |
| Boot/display | Not tested | — | — |
| Keyboard/pointer/gestures | Not tested | — | — |
| USB workspace and filesystem | Not tested | — | — |
| Sound/routes | Not tested | — | — |
| Brightness | Not tested / not applicable | — | — |
| Temperature/fan telemetry | Not tested | — | — |
| Idle/load behavior | Not tested | — | — |
| Battery/AC | Not tested / not applicable | — | — |
| Wired networking | Not tested | — | — |

## Local boot/test handoff

Reference the local run record rather than committing credentials or copying
another host's UUID. That record must contain workspace UUID, installed/current
and previous releases, independent accepted backup, staged manifest, source
commit/dirty state, VM result paths, pre-run session baseline, reboot permission
and armed state, exact checks, post-return collection and screenshots.

## Latest physical result

Date, release/source, exact actions, user observations, log attention markers,
filesystem check, automatic/manual return, accepted scope and failures. Keep
VM, decoder tests and physical results separate.

## Next actions

Prioritized bounded changes, prerequisites, shared-code coordination, required
hardware checks, and recovery steps if the next boot fails.
