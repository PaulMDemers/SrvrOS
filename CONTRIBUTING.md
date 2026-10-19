# Contributing to SrvrOS

Start with the [README](README.md), [current status](docs/status.md),
[architecture](docs/architecture.md) and [roadmap](docs/roadmap.md). The project is
an experimental OS, and the current focus is a useful desktop on the reference
MacBook Air without losing its shell/server foundation.

## Set up and choose a change

Follow [development setup](docs/development.md) for Linux/QEMU. Physical-device
work also needs the [Ubuntu device guide](docs/ubuntu-device-agent-setup.md),
[AGENTS.md](AGENTS.md) and a record for the actual model. New contributors can
start with reproducible bugs, missing tests, documentation or shared widgets.
Discuss broad architecture and driver changes before investing in a large patch.

Create a focused branch from the integration baseline. Keep unrelated changes
separate and preserve local work. One device agent owns one local checkout;
shared-code work needs coordination, not concurrent staging or VM harnesses in
the same checkout. Many tools reuse `build/review` and `latest` pointers.

## Implementation expectations

- Keep shared UI behavior in the widget library or compositor; avoid copying it
  into individual apps. Preserve keyboard operation and cancellation semantics.
- Bound hardware waits and queues, preserve ownership through cancellation and
  test failed initialization/teardown as well as the successful path.
- Match hardware profiles to captured evidence. Do not widen model, codec or
  firmware gates merely to make another machine initialize.
- Preserve upstream notices and document port modifications. Do not rewrite
  unrelated vendor code or silently update pinned dependencies.
- Include the source/payload identity and distinguish host, VM and physical
  results. A screenshot or successful boot does not prove every feature works.

## Validate the affected behavior

```sh
make check-host
make -j2 all usb-image test-core test-desktop
make smoke
make check-docs
```

Run relevant focused harnesses from [testing](docs/testing.md), sequentially.
Documentation-only edits usually need link checks and review; a new device or
kernel contract needs corresponding failure-path tests. VM passes cannot close
physical acceptance items. Do not reboot a contributor's machine or write its
firmware/storage without its owner's authorization and the recovery workflow.

For screenshots, run `python3 tools/desktop_gallery.py` after building, inspect
the results, and follow [screenshot provenance](docs/screenshots.md). Keep raw
logs, firmware captures, hardware photos, UUIDs and private files out of Git.

## Send a reviewable change

Explain the concrete problem, resulting behavior, relevant tests and remaining
limits. Include a small reproduction and source commit for bug reports. Use the
pull-request template to make host/VM/physical evidence easy to distinguish.
Prefer coherent commits with enough context for someone who was not present
during development. Do not present a local pass as a remote CI pass.

AI-assisted contributions are welcome, but the submitted code and claims still
need review and reproducible evidence. Follow the same source, licensing,
privacy and validation expectations as any other contribution. For sensitive
security findings, follow [SECURITY.md](SECURITY.md).
