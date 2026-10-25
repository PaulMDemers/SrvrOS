# SrvrOS

**A from-scratch x86_64 operating system, growing into a useful desktop for older Macs.**

SrvrOS has its own kernel, Unix-like userspace and native desktop. The first
physical target is the MacBook Air **MacBookAir7,2**. A shell, networking and
small server workloads remain part of the project alongside the desktop.

This is an experimental OS under active development. It boots and runs useful
workflows, but it is not a daily-driver replacement or a production server.
Start in QEMU; hardware support is specific to the devices actually tested.

![SrvrOS Text and Files running in QEMU](docs/assets/screenshots/current/desktop-files-text.png)

*Actual native desktop, captured in QEMU at 1440×900. Sample documents;
no mockup or browser UI. [More screenshots and provenance](docs/screenshots.md).*

## What works today

- **Native desktop:** global menus, dock, launcher, movable/resizable windows,
  maximize/restore, tiling, clipboard, context menus, notifications and screenshots.
- **Twelve apps:** Files, Text, Terminal, Preview, Sketch, Find Files, Calculator,
  System, Network Utility, Settings, Clock and Help. Controls live in a
  [shared widget library](userspace/widgets/README.md).
- **Kernel and userspace:** preemptive scheduling, ring-3 ELF programs, virtual
  memory, a shell with jobs/pipelines/scripts, a growing libc compatibility layer
  and selected source ports. The [portable C/POSIX profile](docs/posix-supported-profile.md)
  describes tested contracts and remaining compatibility limits.
- **Storage and networking:** exFAT, USB mass storage, AHCI, e1000, a small IPv4
  stack and static HTTP serving. The laptop workflow uses RAM for the boot image
  and a separate exFAT USB workspace for persistent files.
- **Reference MacBook Air:** built-in keyboard and trackpad, USB file workflows,
  panel brightness, speaker volume and temperature/fan readings have physical
  evidence. Exact acceptance boundaries are in the [status page](docs/status.md).

The reference Air also has evidence for its opt-in cached battery service,
headphone/PCM playback, matched USB Ethernet and secondary-click gestures.
Acceptance is specific to the recorded profile and test, not every device or
workload. Suspend/resume, Wi-Fi connectivity and Bluetooth remain unfinished.
Other Macs have independent bring-up records; support is never inferred from a
similar enclosure. See the [status page](docs/status.md) and [roadmap](docs/roadmap.md).

## Try it in QEMU

On x86_64 Ubuntu, install the host tools, then build from source:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential git python3 python3-pil xorriso \
  qemu-system-x86 qemu-system-gui ovmf acpica-tools
git clone --recurse-submodules https://github.com/PaulMDemers/SrvrOS.git
cd SrvrOS
make check-host
make -j2 all
make run
```

The build fetches pinned Limine components and a checksum-verified Zig 0.15.2
compiler. Internet is needed for the first build. At the guest's `srv>` prompt:

```text
run /fat/bin/deskd
```

Use **Command/Meta-Space** for the launcher, **F1** for Help, and the **SrvrOS →
Return to console** menu to leave the desktop. The [desktop guide](docs/native-desktop.md)
explains apps, gestures and shortcuts. To try the shell instead:

```text
run /fat/bin/sh
```

`make run` uses the embedded RAM-backed filesystem; changes disappear when the
VM exits. `make run-ahci-net` attaches `build/srvros.exfat` as a writable VM disk
and forwards host port 8080 to guest port 80. Test harnesses use disposable disk
copies. See [development setup](docs/development.md) for build artifacts,
MSYS2, optional locally unpacked tools and troubleshooting.

## Build, test, contribute

```sh
make -j2 all usb-image test-core test-desktop
make smoke
python3 tools/desktop_settings_smoke.py
make check-docs
```

Run VM harnesses sequentially in one checkout. The base smoke suite and native
desktop checks cover different paths; neither proves physical hardware support.
See [testing](docs/testing.md), [contributing](CONTRIBUTING.md) and the
[architecture](docs/architecture.md).

For real hardware, follow the [Ubuntu device setup guide](docs/ubuntu-device-agent-setup.md).
It covers model inventory, an exFAT flash drive, a recoverable GRUB one-shot boot,
exact-image rehearsal and result collection. Keep Ubuntu and a known-good payload
available; the filesystem has no power-loss journal. Hardware agents maintain
[separate machine records](docs/machines/README.md).

## Find your way around

| Directory | Contents |
| --- | --- |
| `kernel/` | Kernel, drivers, scheduler, filesystems and network stack |
| `userspace/` | Applications, libc compatibility and shared UI toolkit |
| `shared/include/` | Kernel/userspace ABI definitions |
| `boot/`, `initramfs/` | Boot configuration and initial files |
| `ports/` | Upstream dependencies and SrvrOS adapters |
| `tools/` | Image builders, inventory, VM tests and hardware workflow |
| `docs/` | Current guides, support records and dated engineering history |

[Documentation index](docs/README.md) · [Support status](docs/status.md) ·
[Roadmap](docs/roadmap.md) · [Release notes](docs/release-notes.md) ·
[Security](SECURITY.md)

## License

Original SrvrOS code is available under the [MIT License](LICENSE).
Bundled dependencies, fonts and tools retain their own notices; see
[third-party materials](THIRD_PARTY.md).
