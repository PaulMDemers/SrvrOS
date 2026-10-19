# Documentation

Start with the [project overview](../README.md). Current guides describe the
present implementation; dated journals and the [history directory](history/)
record earlier decisions and may describe superseded behavior.

## Use and understand SrvrOS

- [Current support and limitations](status.md) — implementation versus VM and physical evidence.
- [Screenshots](screenshots.md) — real guest captures, source identity and reproduction.
- [Native desktop](native-desktop.md) — apps, keyboard shortcuts, gestures and window limits.
- [Architecture](architecture.md) — boot, kernel, desktop, storage and device boundaries.
- [Roadmap](roadmap.md) — priorities and acceptance criteria.
- [Release notes](release-notes.md) — milestone summaries, followed by historical entries.

## Build and contribute

- [Development setup](development.md) — Linux/MSYS2 tools, build and VM commands.
- [Contributing](../CONTRIBUTING.md) — focused changes, tests and review evidence.
- [Testing](testing.md) — host, VM, failure-path and physical validation.
- [Widget library](../userspace/widgets/README.md) — shared controls and app integration.
- [Executable format](executable-format.md), [ports](ports.md), [tool inventory](tool-inventory.md).
- [Security policy](../SECURITY.md) and [third-party materials](../THIRD_PARTY.md).

## Work on a physical machine

- [Ubuntu device setup](ubuntu-device-agent-setup.md) — complete build, exFAT,
  recovery, one-shot boot and evidence workflow.
- [Machine records](machines/README.md) — per-model observations and acceptance.
- [Apple hardware expansion](apple-hardware-expansion-plan.md) — planned siblings.
- [Automated hardware tests](automated-hardware-tests.md) — reports, failure halts and collection.
- [Read-only power telemetry](power-telemetry.md) — measurement scope and Linux policy inventory.
- [uACPI port](../ports/srvros/uacpi/PORT.md) — isolated native probe and separate Q35 runtime fixtures.

## Engineering history

The [October 19 journal](desktop-progress-2026-10-19.md) covers the latest
reliability, input, idle and ACPI work. The [October 18 journal](desktop-progress-2026-10-18.md)
records desktop apps and laptop devices. Earlier dated documents remain useful
for reproductions and rationale, but their staged payloads are not instructions
to install those old builds. The [original desktop plan](desktop-plan-2026-10-17.md)
and [browser study](design/desktop-study.html) explain the visual direction;
the browser study is not the running native desktop.
