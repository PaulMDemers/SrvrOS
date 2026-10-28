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

`sh tools/macos/build.sh test-exfat-import` runs the 123 portable import/claim/lifecycle
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


The same wrapper runs the portable namespace/sidecar interruption sweep:

```sh
sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-exfat-crash test-vfs-time
```

`test-exfat-crash` tests 144 write/flush/half-write boundaries using disposable
synthetic media. It accepts quarantine/refusal after interrupted mutations;
it does not certify real power-loss recovery or replace Linux exfatprogs tests.


`sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-exfat-boot`
adds the 29-case boot/system-stream matrix with disposable synthetic images.
Boot checksums, backup copies and upcase tables are now generated correctly;
rebuild images made by older revisions instead of retaining their unchecked boot
regions. A valid backup is a read-only fallback and is never copied over a damaged
primary automatically. No native media repair is part of the Mac test recipe.


Use `sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-exfat-growth`
for the 161-case directory-extension fault sweep. It uses disposable synthetic
files and accepts dirty/refused imports after uncertain writes. The directed
import suite checks that allocation refusals before extension preserve the
original image and keep the mount writable.


Use `sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-exfat-retirement test-process-output`
for 323 deletion/truncation/metadata fault boundaries and process open-file
retirement checks. The import suite additionally checks full-volume reclamation
and clean low-space refusal. All images are disposable; these results do not
establish physical power-loss recovery or a general space-reservation guarantee.


`sh tools/macos/build.sh test-process-writeback` runs the real process/exFAT
full-volume retry fixture using the prepared Limine headers and an x86_64 host
binary under Rosetta. For its sanitizer variant use
`python3 tools/test_exfat_import.py --process --sanitize --output build/review/writeback-retry/integration-sanitized`.
The ordinary 123-mode import suite remains native ARM64 on this host. Process
writeback retries retain data after proven clean refusal; uncertain mutations
remain sticky and quarantined.


For descriptor inheritance and delayed exit errors, run
`sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-process-output`
with the x86_64 host-compiler wrapper described above. The production helper
fixture covers both exit orders, close-on-exec/reuse, partial inheritance and
stdio setup rollback, plus reader-only dirty-content survival. The October 28
checkpoint uses main `5a94d755` (through PR230); evidence is retained under
`build/review/inheritance-exit/`. This is emulated/host coverage, not native Mac
hardware acceptance or exhaustive concurrent process teardown.

For native TCP lifetime checks, use
`sh tools/macos/build.sh HOST_CC='python3 build/tooling/darwin-host-cc.py' test-net-lifetime test-tcp-accept test-net-worker test-net-device`.
The new lifetime fixture also builds natively on ARM64 with `cc`, the repository
include paths and `-Wl,-dead_strip`; add `-fsanitize=address,undefined
-fno-sanitize-recover=all` for sanitizer coverage. Build matched images before
running `tcp_ready_smoke.py`, `tcpstress_smoke.py` and `netabi_smoke.py`
sequentially. These use disposable guest disks and local host forwarding; their
results do not qualify a physical Ethernet or Wi-Fi adapter. October 28 evidence
is under `build/review/tcp-exit/`.


The ACK/handle follow-on keeps the same `test-net-lifetime` command and expands it
to fifteen modes, including checksummed packet admission across sequence wrap and
retired-handle refusal. Evidence is under `build/review/tcp-ack/`; the original
five failing new modes are separate from the passing wrap/reuse controls. Native
ARM64 sanitizer and x86_64/Rosetta runs cover the same fixture.


For an outbound handshake check without a public web dependency, start a
localhost HTTP server on an available unprivileged port and run
`python3 tools/httpget_smoke.py --host 10.0.2.2 --port PORT --path /fixture --boot-wait 60`.
QEMU user networking routes the guest address to the host. Stop the temporary
server after validation; the helper uses a disposable disk copy.


The receive/terminal follow-on expands `test-net-lifetime` to 23 modes using the
same host commands. ARM64 sanitizer and x86_64/Rosetta evidence is retained in
`build/review/tcp-receive/`, together with original-source failures. Its VMs use
matched images, sequential disposable copies, and the host-local outbound HTTP
gate described above. No physical adapter or native Mac acceptance is implied.


The allocation/partial-ACK checkpoint uses the same commands for 30 lifetime
modes, native ARM64 sanitizers and x86_64/Rosetta execution. Its integration base
is main `7b0cc468` through PR233, merged in `b36bb072`. Evidence is under
`build/review/tcp-retry/`; the outbound fixture uses `/retry-check`. This includes
ordinary build/VM integration of the new AP diagnostic and lifecycle baseline,
not native AP/C3 acceptance. Relevant main-branch host gates are `test-power`,
`test-ap-probe` and `test-lifecycle-wait`.


The peer-window/persist checkpoint uses 38 lifetime modes with the same native
ARM64 sanitizer and x86_64/Rosetta commands. Main `45b8647a` through PR234 adds
only device-evidence docs and is integrated in `82aecaf5`; Air6,1's separately
gated coordinated-C3 changes are not part of this qualification. Evidence is
under `build/review/tcp-window/`, with `/window-check` for local outbound HTTP.
The host fixture supplies deterministic two-endpoint zero-window failure/recovery;
ordinary VM smoke gates provide integration evidence, not exhaustive WAN loss
or native adapter coverage.
