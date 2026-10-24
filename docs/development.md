# Development setup

For a new Ubuntu machine and its local agent, use the complete
[Ubuntu device-agent setup](ubuntu-device-agent-setup.md), including the current
development branch, exFAT drive and recovery workflow. The dated
[hardware development](hardware-development.md) page retains original Air evidence.

The base OS builds on x86_64 Linux and MSYS2 UCRT64. Zig 0.15.2 is downloaded
into `build/tooling/zig`, verified against the release SHA-256, and used for
freestanding compilation. Limine binaries and protocol headers are pinned to
commits in the Makefile. The Linux build compiles Limine's host utility with
`HOST_CC` (default `cc`). No cross GCC installation is required.

## Ubuntu / Debian

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends build-essential git python3 python3-pil xorriso qemu-system-x86 qemu-system-gui ovmf acpica-tools
# From the repository root:
git submodule update --init --recursive
make check-host
make -j2 all usb-image
make smoke
```

`-j2` is suitable for this dual-core MacBook Air. `make run` opens QEMU;
`make run-ahci-net` attaches the generated exFAT disk and forwards host port
8080 to guest port 80. Interactive disk changes persist in that image. Smoke
harnesses use disposable copies. `make clean` removes generated OS images;
`make distclean` removes everything under `build`, including downloaded tools.

When administrative installation is unavailable, use the optional local setup:

```sh
python3 tools/setup_local_host.py
tools/host-env.sh make check-host
tools/host-env.sh make -j2 all usb-image
tools/host-env.sh make smoke
tools/host-env.sh make run
```

This downloads packages using the host's configured APT repositories and
unpacks them in `build/host-tools/root`. It relies on the host's existing
Python, compiler, Git, Make, and installed library dependencies. It does not
run package installation scripts or modify system packages. The wrapper sets
local binary/library/firmware paths only for the invoked command. It is an
Ubuntu/Debian convenience, not a portable substitute for a package manager.

## First-build troubleshooting

If a download is interrupted, inspect the named staging/output directory before
retrying. The Zig bootstrap verifies its archive hash and version and refuses an
already-existing incomplete output; move that incomplete directory aside rather
than disabling verification. Do not delete all of `build` if it contains needed
hardware evidence or recovery artifacts.

A clean parallel-build ordering bug in the ACPI object rule was corrected on
2026-10-19: those objects now wait for the compiler and protocol headers instead
of racing the toolchain download. `make test-make-bootstrap` reproduces that
ordering with a delayed disposable compiler across native and alternate ACPI
object rules. It does not remove or alter the installed host toolchain.

If QEMU reports no usable graphical display on a headless host, use the documented
hidden VM harnesses rather than `make run`, which opens a window. For UEFI tests,
install OVMF or set `OVMF_CODE` to the actual firmware path. Do not substitute a
physical disk for a test image to work around a VM setup error.

## MSYS2 UCRT64

Use the original UCRT64 development tools (`make`, Git, Python 3, `xorriso`,
QEMU and a host C compiler). Run the same Make targets. Windows uses the Zig
Windows archive and Limine's supplied `limine.exe`. Set `PYTHON=python` if the
installation does not expose `python3`.

## Verification

`make check-host` checks prerequisite programs, UEFI firmware, and submodule
checkout presence. A `CHECK` prefix on an initialized submodule means its HEAD
differs from the gitlink; inspect it before committing. It never resets files.

`make check-docs` checks repository Markdown file destinations and the current
screenshot manifest without accessing the network. It does not check remote
URLs or heading anchors. To regenerate actual native desktop images after a
build, run `python3 tools/desktop_gallery.py`; see [the gallery](screenshots.md)
for review and publication steps. Pillow is needed for that capture tool, not
for the ordinary kernel build.

`make smoke` builds both images, checks serial-harness synchronization with
unit tests, and runs these QEMU harnesses sequentially:

1. Syscall ABI validation.
2. Shell/CLI regression coverage.
3. Shell editing, including literal brackets and ANSI cursor/history keys.
4. UEFI GPT/FAT32 boot with an `echo usb-input-ok` command typed through
   the emulated xHCI keyboard.
5. The displayd desktop smoke.

The baseline allows up to 180 seconds for boot (`SMOKE_BOOT_WAIT` overrides
this). The original 45-second timeout predates the console/timing work. The
current kernel calibrates its LAPIC timer against the ACPI PM reference; a
reference/calibration failure on another machine still needs explicit diagnosis.

`make libc-audit` checks declared libc functions against exported symbols.
The GitHub Actions workflow runs the base Linux build and smoke suite on
Ubuntu 24.04 and retains logs. Local passing tests do not imply that the remote
workflow has run, that all other harnesses pass, or that physical hardware works.

TCP listener semantics and the directed host/VM regressions are documented in
[TCP readiness](tcp-readiness.md). Run network fixtures separately from the
baseline smoke suite; a passing baseline does not cover them.

UEFI harnesses accept `OVMF_CODE=/path/to/firmware.fd` or `--ovmf`. Linux OVMF
locations and the existing MSYS2 locations are searched by default.

The optional custom A1466 QEMU build helper has Linux path support, but its
full source build was not run during this setup pass. It requires extra development dependencies
(e.g. `ninja-build`, `pkg-config`, `libglib2.0-dev`, `libpixman-1-dev`, Python
venv support and iASL). Its hardware identity assumptions need correction;
read the review below before using it as hardware evidence.

## Node port recovery

The incoming repository at `7d13270` recorded Node gitlink
`ff6ef2a7da976fb0a06cf75da8ea0ca75a385163`. GitHub's configured upstream
`nodejs/node` rejects fetching that object (`not our ref`). Earlier project
commits also reference a sequence of apparently local Node commits.

This setup change restores the documented upstream Node v24.16.0 commit
`c7d10158bc31036de6783d66beaaaf551e3167aa` so a recursive clone is possible.
The tracked patch queue remains in `ports/srvros/node/patches`; its initial
patch passes `git apply --check` against that release, but it must not be
assumed to contain every change in the missing local commits. The upstream
checkout is left unpatched and clean.

Recover the old Node repository or a Git bundle from the previous development
machine before resuming the full Node runtime port. Preserve the missing
commit's changes as tracked patches or publish them to an accessible fork.
The default OS image and baseline tests do not build the full Node runtime;
prior Node runtime milestones are not reproduced by this setup pass.

See [the October 2026 review](project-review-2026-10-16.md) for the physical
hardware findings and validation results.
