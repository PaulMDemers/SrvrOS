# Per-machine development records

Follow the [coordination guide](../agent-coordination.md) for shared ownership,
PR integration and handoff checkpoints.

One agent develops locally on each Ubuntu machine. Start with the
[Ubuntu device-agent guide](../ubuntu-device-agent-setup.md), then update the
matching record before the first physical trial. The
[expansion plan](../apple-hardware-expansion-plan.md) is shared planning; these
records own observed configuration and acceptance for each model/unit.

| Model | Record | Current evidence |
| --- | --- | --- |
| MacBookAir7,2 / A1466 | [macbookair7-2](macbookair7-2.md) | Active reference machine; partial device acceptance |
| MacBookAir7,1 / A1465 | [macbookair7-1](macbookair7-1.md) | Planned; no local inventory or physical results |
| MacBookPro11,2 / A1398 | [macbookpro11-2](macbookpro11-2.md) | Console boot, keyboard enumeration, USB log persistence and automatic return verified; typing pending |
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
