# Build and run on Apple Silicon macOS

SrvrOS is an x86_64 OS. ARM64 macOS builds use native ARM64 Zig and run in
QEMU's TCG software emulator. This is not an ARM64 kernel or native Apple Silicon
hardware support. The [host record](machines/apple-silicon-build-host.md) records
the initial MacBookPro18,2 trial and subsequent checks.

## Install, build and run

Install Apple's command-line developer tools if absent (`xcode-select --install`).
Install [Homebrew](https://brew.sh/) using its official instructions, including
its shell PATH setup, then run:

```sh
brew install qemu xorriso gnu-tar make python
git clone --recurse-submodules https://github.com/PaulMDemers/SrvrOS.git
cd SrvrOS
sh tools/macos/build.sh check-host
sh tools/macos/build.sh -j2 all
python3 tools/macos/run.py
```

Use Homebrew Python 3.12 or newer. For an existing checkout, initialize the pinned
dependencies with `git submodule update --init --recursive`, without `--remote`.
Internet is needed for the initial tools. The wrapper discovers the Homebrew
prefix, selects GNU Make 4 and GNU tar, and locates QEMU's x86_64 EFI firmware.
Apple's bundled Make 3.81 can select a kernel compile rule for userspace and fail
to find `unistd.h`; GNU tar supplies the initramfs ownership/USTAR options.

The Makefile downloads checksum-pinned Zig **0.15.2** for ARM64 macOS under
`build/tooling/zig` and pinned Limine components. No Rosetta, x86 Linux VM or cross
GCC is required. The wrapper keeps Zig caches under `build/tooling/`, disables
macOS copy metadata in the archive environment and does not edit shell startup
files. Run builds and VM harnesses sequentially in one checkout.

## Interactive VM and controls

The launcher uses Q35, one emulated x86_64 CPU, 1 GiB RAM, a 1440×900 Cocoa window,
USB keyboard/mouse, writable AHCI exFAT storage, e1000 user-mode networking and
HDA output through CoreAudio. It configures no host folder sharing, incoming
port forwarding or microphone capture. Native battery/EC/brightness profiles
are not enabled. The guest clock is UTC.

It boots the BIOS ISO, starts `/fat/bin/deskd --apps-trace`, waits for the first
application frame, saves a screenshot and leaves QEMU running. Finding EFI
firmware in `check-host` is not a UEFI boot test. Use the dock or Command/Meta-Space
launcher; F1 opens Help. Ctrl-S in an untitled Text document opens Save As, which
must be completed. QEMU's View menu shows its current mouse-release shortcut.

To finish, save work and choose **SrvrOS → Return to console** so app workers
can drain, then close the QEMU window. Returning to the console does not stop
the VM. The monitor has no `shutdown`, `quit` or `exit` command. Enter
`run /fat/bin/deskd` to reopen the desktop in the same guest.

## Saved files and provenance

VM state defaults to `.local/macos-vm/` in the checkout. It is ignored by Git and
survives both `make clean` and `make distclean`. To choose another location:

```sh
python3 tools/macos/run.py --state-dir "$HOME/SrvrOS-VM"
```

`/fat/home` resides on the writable VM disk. The same image fingerprint reuses
its disk; changed kernel/initramfs/template hashes select a new disk and retain
the old one. Guest files are not automatically migrated. Back up wanted disks
while QEMU is stopped. Deleting this state directory deletes those guest files.

Each session retains its boot ISO, serial/QEMU logs, `desktop.ppm` and manifest.
`current.json` identifies the latest session, PID, disk, command, timing and
socket directory. `build.sh ... all` records `build/macos-build.json` after a
successful real build (dry-run/question/touch modes are refused for `all`);
the launcher verifies its hashes and uses that recorded source,
not the current checkout's commit. Dirty builds and the tracked-diff digest are
recorded. This is provenance, not an archive of uncommitted source; retain source
separately when reproducing a dirty build.

Starts are serialized per state directory. A live PID causes refusal, never a
kill. If an old PID was reused, inspect it and remove only stale `qemu.pid` after
confirming the prior VM is gone. Short private `/tmp/srvros-*` socket directories
avoid macOS UNIX path limits; remove only the matching directory after VM exit.
A boot timeout leaves the VM and logs available for diagnosis.

## Verification and limits

```sh
python3 tools/test_macos_host.py -v
sh tools/macos/build.sh check-docs
python3 tools/macos/run.py --headless --state-dir /tmp/srvros-macos-check
# Stop this disposable test instance after inspecting its logs:
kill -TERM "$(cat /tmp/srvros-macos-check/qemu.pid)"
```

Host tests cover platform selection, stale-image refusal, preserved guest writes,
failed-copy cleanup, active-PID refusal and refusal to record simulated builds. `--headless` uses no window or host
audio but still leaves the VM running; its manifest records the PID/QMP socket.
Use a separate state directory for directed tests.

The first interactive trial used QEMU 11.1.2, GNU Make 4.4.1, Zig 0.15.2 and
source `46f6212f`. Desktop readiness took 6.61 seconds in that run; this is one
startup observation, not a performance benchmark. The user confirmed opening,
running and testing the VM.

The full [testing suite](testing.md) is not qualified on macOS. Some host fixtures
use x86 assembly or ELF linker flags; some VM harnesses invoke Linux filesystem
or GRUB tools and paths. A successful build/boot does not imply those suites
passed. Use x86_64 Ubuntu for those gates until individual macOS paths are validated.

For PR175's synchronization corrections, the complete `tools/thread_smoke.py`
(with `--boot-wait 60 --run-wait 300`) and `tools/libuv_smoke.py` (with
`--boot-wait 60`) passed on this Mac, sequentially, against rebuilt images.
Those selected harnesses use disposable disks and stop their own QEMU instances.
`sh tools/macos/build.sh test-sync-contracts` runs the associated host regressions.
This does not qualify every `test-core` target or other guest/hardware harness.

### Rebuild images after entry-integrity changes

The exFAT import parser now refuses unset or incorrect entry-set checksums and
unsupported stream/name layouts. Older generated disks must be rebuilt with
`sh tools/macos/build.sh -j2 all` and paired with the matching kernel. This rebuilds
generated build artifacts; it is not an upgrade/repair procedure for persistent
workspace or recovery media. Run `sh tools/macos/build.sh test-exfat-import` and
see the [filesystem audit](filesystem-audit-2026-10-26.md) for the supported ASCII
entry subset, regression evidence and remaining gaps.

Block mounts now check allocation ownership before allowing writes. Inconsistent
maps or failed checks remain readable but cannot be mutated through exFAT; inspect
the `exfat-check` diagnostics and recover offline. Generated exact allocation
aliases are retained safely until their last owner is removed. Mount paths must
be canonical, unoccupied and outside other exFAT mount subtrees.

## Focused storage host tests

`sh tools/macos/build.sh test-exfat-import` runs the 51 portable import/claim
modes; `python3 tools/test_exfat_import.py --sanitize` adds ASan/UBSan. The
storage adapter aggregate also includes an existing x86 USB fixture. On this
Mac it was run with Rosetta and the following local compiler wrapper, which
translates the GNU dead-section linker option for Darwin. Rosetta is an extra
requirement for this host-test recipe, not for the OS build or QEMU launcher.

```sh
mkdir -p build/tooling
cat > build/tooling/darwin-host-cc.py <<'PYTHON'
import os, sys
os.execvp("cc", ["cc", "-arch", "x86_64"] +
          [arg.replace("-Wl,--gc-sections", "-Wl,-dead_strip") for arg in sys.argv[1:]])
PYTHON
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-storage-leases
```

The new image tests use disposable synthetic regular files under
`build/review/storage-claims/image/`; no existing workspace disk is opened.
The full Linux-oriented host suite remains unqualified on macOS. In particular,
these results do not accept the separate `test-workspace-image` exfatprogs
power-cut sweep or the unrelated sync-command host fixture.
