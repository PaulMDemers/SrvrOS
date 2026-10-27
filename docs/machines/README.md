# Per-machine development records

Follow the [coordination guide](../agent-coordination.md) for shared ownership,
PR integration and handoff checkpoints.

One agent develops locally on each Ubuntu machine. Start with the
[Ubuntu device-agent guide](../ubuntu-device-agent-setup.md), then update the
matching record before the first physical trial. The
[expansion plan](../apple-hardware-expansion-plan.md) is shared planning; these
records own observed configuration and acceptance for each model/unit.

The [Apple Silicon build host](apple-silicon-build-host.md) has a separate
[macOS setup](../macos-development.md). It builds and emulates x86_64; its VM
results do not add a native ARM64 machine to the hardware support table.

| Model | Record | Current evidence |
| --- | --- | --- |
| MacBookAir7,2 / A1466 | [macbookair7-2](macbookair7-2.md) | Active reference machine; partial device acceptance |
| MacBookAir6,1 | [macbookair6-1](macbookair6-1.md) | Native desktop/input, bounded concurrent USB work, rerun refusal and speed/acceleration persistence accepted; brightness 5–100% and bounded speaker/headphone playback accepted; parked battery sampling, AC transitions, observed alarm refresh/retirement and bounded battery/Text USB-image coexistence and short mounted Home sampling accepted; bounded Home Text/Start/Stop and three-minute continuous desktop capture and attended AC indicator updates accepted; default battery startup remains off; opt-in ordinary monitoring and a 22-minute combined battery/brightness/audio/sensor session accepted with file persistence and clean retirement; automated and attended combined reopen accepted after integrity-preserving heap optimization (four attended lifetimes, 3.60–3.79 s startup, Text/Files work and clean retirement); bounded 30-minute audio/disk/app stress with three screenshots passes 703 app cycles and clean exit under unchanged limits; read-only CPU package power/active-frequency console and battery-owned desktop idle/app/recovery phases accepted, with 13 automated app cycles, valid power/temperature/worker coverage and clean retirement; event-driven desktop worker waits accepted with zero lifecycle timeout wakes in idle/apps/recovery; measured package power remains about 4.1 W; battery supervisor event waits also accepted with zero timer wakes and no covered idle/recovery switches; read-only hardware idle residency capture accepted across 30-second idle/load/recovery phases, with valid zero core C3/C6/C7 and package C2–C10 counters; deeper-state entry, longer endurance and CPU policy remain pending |
| MacBookAir7,1 / A1465 | [macbookair7-1](macbookair7-1.md) | Planned; no local inventory or physical results |
| MacBookPro11,2 / A1398 | [macbookpro11-2](macbookpro11-2.md) | Native apps, input, brightness, sound and read-only CPU/two-fan telemetry and captured USB Ethernet cable recovery/UI exercised; trackpad latency work deferred |
| MacBookPro11,3 / A1398 | [macbookpro11-3](macbookpro11-3.md) | Planned; no local inventory or physical results |
| Macmini7,1 / A1347 | [macmini7-1](macmini7-1.md) | Planned; no local inventory or physical results |

Use the [record template](TEMPLATE.md). Put portable observations, source
commits and acceptance summaries in Git. Keep raw inventories, images, local
mount paths, UUID/reboot state and full logs in the machine's ignored `build/`
records, with accepted recovery copies outside `build`. Do not commit secrets,
serial numbers or private documents. If there are multiple units of a model,
give each its own labeled record rather than overwriting another unit's evidence.

Before ending a work block or rebooting, record what is installed, what will run,
where evidence will be saved, whether reboot is armed, and what the next agent
turn should do. On return, collect first and clear stale planned/armed state only
after checking the host. Scripts do not provide cross-machine orchestration.

Updates to shared code should state the touched subsystem, base commit, tests,
physical evidence and remaining unknowns. Integrate through the shared branch's
owner/review process; other machines should record the new base and rerun relevant
regressions. An agent should not assume remote changes or another device's work
have landed until it fetches and inspects the actual commits.
