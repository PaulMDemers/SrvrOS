# MacBookAir7,2 / A1466

Updated: 2026-10-18. State: **active reference machine; partial device acceptance**.

## Identity and ownership

Development host: Ubuntu on the 13-inch Air, Broadwell-U, Intel HD Graphics 6000
`8086:1626`, xHCI `8086:9cb1`, Apple topcase `05ac:0291`, HDA `8086:9ca0` /
CS4208 `1013:4208` subsystem `106b:7e00`. Firmware, descriptors and routing evidence
are in the linked device guides. The shared integration baseline is now `main`,
including the completed `codex/linux-development-baseline` work. Subsequent
device work uses separate branches based on `main`.

## Accepted scope and open items

| Capability | State | Evidence / limit |
| --- | --- | --- |
| Desktop boot, rendering, keyboard, trackpad, USB documents | Physically exercised | User rounds and durable workspace history in the development journal |
| Held-click second-finger steering | User accepted | `d61e60a82a295d84`; counters do not independently prove every handoff |
| Brightness and audible volume adjustment | User accepted | `b427b78f21644bd2`; requested brightness 16/83 applied, three tone tests completed |
| Temperature and fan sampling | Live output confirmed | Fresh screenshot readings 83 C / 1202 RPM; logs reached 85 C, no SMC errors; calibration/power behavior not accepted |
| Two-finger physical right-click | Implemented; physical acceptance pending | Press role latches from initial contact count; one-finger press plus later steering finger remains primary |
| Context menu from trackpad taps | Physical acceptance open | Exit counts 5 primary and 1 secondary tap; user reported no visible right-click result |
| System window stability | Failure recorded | `input-timeout queue=5` closed System pid14 in the latest trial |
| PCM files / headphones | VM-tested / physical confirmation incomplete | Volume report does not separately accept these routes/workflows |
| Battery/AC and suspend/resume | Not implemented as live services | ACPI/EC foundations exist; runtime ownership and tests remain |
| Optional AML probe | Native stall | `fe119d1c99070b18`; v22 omits the probe, root cause unresolved |
| USB Ethernet | Implemented CDC ECM, VM-tested | No actual adapter accepted |

## Latest physical record

Release `b427b78f21644bd2`, v22, built from source `5b1ed05`.
Session `session-00065e281812e807-00` has saved logs and clean desktop exit.
Local evidence is in `build/native-desktop-trials/b427b78f21644bd2/physical-after/`.
The detailed [journal](../desktop-progress-2026-10-18.md) preserves failures,
partial acceptance and the previous implementation sequence.

At this checkpoint current is `b427b78f21644bd2`, previous is `f05dfcc01c28dc85`,
and the independent held-drag backup is under
`~/.local/share/srvros/known-good/d61e60a82a295d84`. Re-read actual installed state
before any action; these are historical identifiers, not values for other Macs.
The local trial record marks this run returned/reviewed, not armed for reboot.

## Next actions

1. Physically accept two-finger secondary click while preserving established primary drag; host/transport tests precede this check.
2. Reproduce and fix the System input timeout.
3. Measure idle/polling/load behavior before selecting power optimizations.
4. Battery/AC runtime, headphone route acceptance and actual USB networking.
5. Shared inventory/profile/scaling work for the other model agents, coordinated
   through the [expansion plan](../apple-hardware-expansion-plan.md).
