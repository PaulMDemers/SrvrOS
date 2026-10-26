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
- [Apple Silicon macOS](macos-development.md) — ARM64 compiler, x86_64 QEMU and persistent VM disks.
- [Contributing](../CONTRIBUTING.md) — focused changes, tests and review evidence.
- [Testing](testing.md) — host, VM, failure-path and physical validation.
- [Widget library](../userspace/widgets/README.md) — shared controls and app integration.
- [Portable C/POSIX profile](posix-supported-profile.md) — supported contracts, bounds and consumer tests.
- [Synchronization audit](synchronization-audit-2026-10-25.md) — confirmed stack, clock and semaphore defects with reproducible host probes.
- [Executable format](executable-format.md), [ports](ports.md), [tool inventory](tool-inventory.md).
- [Security policy](../SECURITY.md) and [third-party materials](../THIRD_PARTY.md).

## Work on a physical machine

- [Agent coordination](agent-coordination.md) — ownership, shared changes, integration and checkpoints.

- [Ubuntu device setup](ubuntu-device-agent-setup.md) — complete build, exFAT,
  recovery, one-shot boot and evidence workflow.
- [Machine records](machines/README.md) — per-model observations and acceptance.
- [Apple hardware expansion](apple-hardware-expansion-plan.md) — planned siblings.
- [Automated hardware tests](automated-hardware-tests.md) — reports, failure halts and collection.
- [Read-only power telemetry](power-telemetry.md) — measurement scope and Linux policy inventory.
- [uACPI port](../ports/srvros/uacpi/PORT.md) — offline/isolated probes, Q35 fixtures and separately gated native runtime.

## Engineering history

The [October 25 code/documentation audit](code-docs-audit-2026-10-25.md) records
current-guide corrections and outstanding review items. Current acceptance lives
in [status](status.md) and the machine records. The
[October 19 journal](desktop-progress-2026-10-19.md) records that date’s
reliability, input, idle and ACPI work. The [October 18 journal](desktop-progress-2026-10-18.md)
records desktop apps and laptop devices. Earlier dated documents remain useful
for reproductions and rationale, but their staged payloads are not instructions
to install those old builds. The [original desktop plan](desktop-plan-2026-10-17.md)
and [browser study](design/desktop-study.html) explain the visual direction;
the browser study is not the running native desktop.
