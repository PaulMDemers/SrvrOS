---
name: srvros-trial-return
description: Collect or replay evidence from a recorded SrvrOS native laptop trial after Ubuntu returns, including a “back” from an outstanding trial.
---

Recover the exact run directory from the unit's pre-boot handoff. If two
runs could be outstanding, reconcile the model, release and workspace UUID
before choosing one. The directory contains `run.json` and either an inline
pre-boot session list or `workspace-before.json`.

From the SrvrOS checkout, run `tools/trial collect RUN --save` for a
returned desktop or laptop-probe session on a mounted USB workspace.
Use `tools/trial collect RUN --archive` to replay saved evidence.
For an unattended filesystem-confidence batch use its own collector in
`tools/hardware_test.py`. The [device guide](../../../docs/ubuntu-device-agent-setup.md#7-test-return-and-collect)
describes both routes and the exit codes.

Read each finding against the intended test. A verified exit snapshot shows
that the final bytes were captured. For Wi-Fi, use the recipe's verifier to
distinguish association, data transfer and teardown. For display scaling,
distinguish the firmware-selected hint from saved Settings and the actual
startup scale. Pair checks such as keypresses, pointer alignment and
audible sound with the operator's observations. If a trial stopped early,
record the last proved stage, the outstanding checks and the recovery
evidence in the unit handoff. Preserve diagnostic failures on retries.
