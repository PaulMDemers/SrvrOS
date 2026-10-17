# Project review — 2026-10-16

## Scope and repository

Reviewed the boot/build path, architecture and test documentation, input driver
binding and custom emulator assumptions, and Linux's device inventory on the
actual target laptop. This was development setup and baseline validation, not
a comprehensive correctness/security audit or a physical SrvrOS boot attempt.

Starting point: `main`, commit `7d13270`, synchronized with `origin/main`.
Changes remain local and uncommitted on `codex/linux-development-baseline`.
The replacement Node gitlink is staged so `git submodule update` uses the
recoverable upstream commit in this checkout. zlib and Node contained only
`.git` files and empty indexes after incomplete clone operations; there were
no source edits in those directories to preserve. zlib was restored to its
recorded v1.3.2 commit. Node required the recovery described in
[development setup](development.md#node-port-recovery).

The project already contains substantial OS infrastructure: a freestanding C
kernel, Limine BIOS/UEFI boot, memory protection and scheduling, ring-3 ELF
programs, a shell/libc layer, exFAT, AHCI, e1000 networking, xHCI HID support,
and the displayd userspace compositor. Numerous serial/QMP smoke harnesses
exist. Build and runtime evidence is more useful than the breadth of the
feature list when choosing the next milestone.

## Hardware findings requiring attention before physical boot

The running host identifies itself as **MacBookAir7,2**. Read-only `lspci`,
`lsusb`, `/proc/bus/input/devices`, and sysfs inspection show:

| Device | Observed identity and active Linux driver |
| --- | --- |
| Internal keyboard | USB `05ac:0291`, bus 1 root port 5, `usbhid` |
| Internal trackpad | Same USB composite device, interface 2, `bcm5974` |
| xHCI controller | `00:14.0`, `8086:9cb1`, `xhci_hcd` |
| Management engine | `00:16.0`, `8086:9cba`, `mei_me` |
| GSPI controller | `00:15.4`, `8086:9ce6`, `pxa2xx_spi_pci` |
| LPSS DMA | `00:15.0`, `8086:9ce0`, `dw_dmac_pci` |
| ACPI SPI topcase child | `spi-APP000D:00` below `00:15.4`, no bound driver |
| Internal SSD controller | `04:00.0`, `144d:a801`, `ahci` |
| Wi-Fi | Broadcom `14e4:43a0`, `wl`; SrvrOS's e1000 driver does not cover it |

The presence of an ACPI `APPLE-SPI-TOPCASE` object therefore does not prove
that working keyboard events use SPI. USB is the active path in this Linux
session; firmware/OS initialization differences at SrvrOS boot still need
investigation. The keyboard is not behind the Bluetooth hub in this topology.

**High-priority finding in the incoming code:** `kernel/src/drivers/lpss_spi.c`
defined its SPI controller as `0x9cba`, bound by that ID, and mapped MMIO when
PCI memory decoding was already enabled, then wrote LPSS-specific registers.
On this laptop those accesses targeted MEI.
`kernel/src/arch/x86_64/pci.c` also labelled that identity as an input candidate.
This invalidates the earlier interpretation of its register dumps.

The custom model in `ports/qemu-a1466/src/a1466-lpss.c` reproduces the same
incorrect identity at `00:16.0`, so its passing tests can only establish
agreement between the driver and the synthetic model. They cannot establish
correct hardware binding or transport behavior.

Independent primary-source corroboration:

- [Linux MEI device IDs](https://github.com/torvalds/linux/blob/master/drivers/misc/mei/hw-me-regs.h) identify `0x9cba` as Wildcat Point LP MEI.
- [Linux PXA2xx SPI PCI driver](https://github.com/torvalds/linux/blob/master/drivers/spi/spi-pxa2xx-pci.c) is the reference for the actual GSPI binding.

Local raw evidence: `build/review/linux-hardware.txt`. It contains no device
serial-number dump. The summary here is tracked because `build/` is disposable.
No device driver was unbound, no PCI register was written on the host, and no
removable media or internal disk was flashed.

## Storage and test boundary

The UEFI USB-image harness attaches the GPT image as an **AHCI disk** and adds
xHCI HID devices separately. It verifies the image format, UEFI boot, and USB
input, but does not test runtime USB mass storage. The current mount selection
in `kernel/src/main.c` tries `ahci0`, its GPT partitions, then
`initramfs-exfat`; there is no USB mass-storage block driver in this tree.
Consequently, firmware loading a USB stick does not imply that SrvrOS can
persist logs back to that stick. Verify `/fat`'s actual backing device during
hardware bring-up, and plan a capture path that does not depend on writable
USB storage or a working built-in keyboard.

## Setup changes

- Fixed a CLI-discovered line-editing bug: bare `[A`/`[B`/`[C`/`[D` text
  was consumed as cursor/history input without an ESC prefix, corrupting regex
  commands. Added literal-bracket coverage alongside actual ANSI-arrow tests.
- Gave the CLI harness separate ordinary-command and long-POSIX-probe timeout
  limits, slower serial pacing, prompt-at-end matching, and progress output.
  Help text containing example prompts no longer signals command completion.
  The original harness sent
  more commands before long help loops completed and cut off the POSIX probe.
- Fixed the generated Lua source/stamp dependency so unchanged builds are no-ops.
- Host-aware Zig/Linux executable paths; SHA-256-verified compiler bootstrap.
- Pinned Limine binary/protocol commits and native Linux host utility build.
- Restored dependency checkouts, with explicit Node recovery limitations.
- Ubuntu/OVMF lookup in UEFI harnesses and optional repository-local host tools.
- `make check-host`, `make smoke`, Linux setup documentation, and a GitHub
  Actions build/smoke workflow.
- Prominent corrections on the older A1466 notes. Kernel/device behavior is
  unchanged in this setup pass; the shell editing correction is in userspace.

## Validation

Validated locally with Zig 0.15.2, Limine 11.4.1, and Ubuntu QEMU 10.2.1:

| Check | Result |
| --- | --- |
| Clean first build of kernel/userspace/ISO | Passed |
| GPT/FAT32/exFAT USB image generation | Passed |
| Unchanged `make all usb-image` | No work required |
| Host prerequisites and firmware lookup | Passed using local host-tools wrapper |
| libc declared/exported symbol audit | 753 declarations, 0 missing exports |
| BIOS/syscall ABI smoke | Passed with `--boot-wait 120` |
| UEFI/xHCI keyboard smoke | Passed; 36 keyboard reports and executed `echo usb-input-ok` |
| CLI regression | Passed all 479 commands after corrections; POSIX probe completed; final fsck errors=0 |
| Shell editing regression | Passed; literal brackets and ANSI arrows, fsck clean |
| Serial harness unit regressions | 3 passed; help examples do not terminate prompt waits |
| displayd smoke | Passed; clients launched and compositor exited cleanly |
| Python syntax, shell syntax, Git whitespace checks | Passed |

The original ABI harness's 45-second boot allowance timed out during init;
its retry reached the monitor and passed. `make smoke` uses a configurable
180-second boot ceiling. No kernel timer behavior was changed to satisfy it.
The CLI/UEFI/displayd tests use the same 180-second ceiling. ABI, UEFI, and
displayd passed before the subsequent shell-only correction; CLI and editing
regressions validate that correction on the rebuilt images.

Final logs are `build/review/cli-final.log`, `shell-edit.log`,
`sysabi-retry.log`, `uefi-usb.log`, `displayd.log`, `libc-audit.log`, and
`harness-unit.log`. Earlier failing/interrupted runs remain for diagnosis.
The artifact SHA-256 manifest is `build/review/artifacts.json`. The custom
SPI emulator, full Node runtime, physical boot, Windows compatibility, and the
remote GitHub Actions run have not been validated in this pass.

## Next phase

1. The follow-up below gates SPI and removes the `9cba` match. Add a negative
   regression ensuring MEI is never claimed as SPI before enabling that path.
   Do not simply redirect all existing writes to `9ce6` without validating
   controller resources and initialization requirements.
2. Compare SrvrOS xHCI port discovery, firmware ownership handoff, port reset,
   address/configuration, and HID interface handling against Linux's known
   `05ac:0291` root-port-5 topology. Capture diagnostics without requiring the
   internal keyboard to work.
3. Establish external USB keyboard input and reliable capture storage, then
   internal keyboard input. Treat the bcm5974 trackpad as a separate protocol
   task; generic USB mouse success does not prove its support.
4. Correct the custom emulator's identities and scope against independent
   evidence before using it for further SPI development. Keep firmware mode
   switching/ACPI execution as an evidence-driven investigation.
5. Calibrate the LAPIC timer against a reference clock. The current fixed
   initial count is not a portable guarantee of 100 Hz, although callers use
   ticks as time units.
6. Recover the missing Node port history independently of hardware bring-up.

## Follow-up: on-laptop build/reboot workflow

The subsequent hardware-workflow change adds `make hardware-stage`,
`make hardware-smoke`, a verified EFI/GRUB installer and a read-only Linux
hardware collector. See [hardware development](hardware-development.md) for
commands, exact input interfaces and the bootloader path workaround.

SPI is now disabled by default; explicit experimental initialization matches
GSPI `9ce6` with class `0c80` instead of MEI. The dedicated hardware entry uses
`srvros.storage=ram`, skips AHCI initialization and mounts the bundled filesystem
in RAM. Ordinary ISO entries retain their existing storage policy. This is a
change from the earlier setup-only baseline above.

The staged release passed a real GRUB → Limine → SrvrOS OVMF boot, RAM filesystem
checks and a command typed through QEMU's USB keyboard. Six installer unit tests
passed, including rollback and previous-release retention. The rebuilt ordinary
BIOS/AHCI image also passed the syscall ABI smoke (`hardware-sysabi.log`).
`build/review/hardware-validation.json` records the new artifact hashes. Initial Linux capture
includes Apple USB/HID descriptors; protected ACPI tables still require sudo.
The installer host preflight confirms EFI FAT UUID `7000-79A2`, and this Mac
reports Secure Boot unsupported. Host installation has not been performed: sudo
requires interactive authentication. No physical reboot has been attempted.

The release-specific manifest under `build/hardware-boot/` identifies the tested
payload. Earlier baseline logs describe the earlier kernel; consult the new
`hardware-*` logs for these changes.

## Physical boot feedback and first USB corrections

The user subsequently installed release `1feaf98d0634e58b` and booted it on the
MacBookAir7,2. It reached the monitor, but the built-in keyboard did not respond
either during the input probe or at the prompt. No external keyboard was tried.
After a forced restart Ubuntu was available again. This supersedes the earlier
"not installed/not physically booted" status above; there is no retained kernel
log from that attempt because the test filesystem was in RAM.

The xHCI review found missing firmware ownership negotiation and scratchpad
allocation, a raw USB bInterval incorrectly used as an xHCI exponent, and zero
Max ESIT Payload for interrupt endpoints. Connected/enabled USB2 devices also
kept firmware's old USB address/configuration because their ports weren't reset.
These are corrected in the new candidate along with short-packet event flags,
polling interrupt masking, DMA publication/consumption ordering, failed-endpoint
poll suppression, and checking boot-protocol request failures. Their presence
in the code does not prove which defect caused this physical boot failure.

A captured 84-byte Apple USB configuration descriptor now exercises the actual
driver parser and context writer in `make test-xhci`. The hardware boot entry
also leaves a compact diagnostic screen above the prompt so the next test can
be diagnosed without a working keyboard. See [hardware development](hardware-development.md)
for deployment and capture instructions. The installer accepts the prior config
version when retaining the old release as a fallback.

Candidate `ad2f72b629a8797a` is staged. The kernel/image build and host driver
regressions passed; the GRUB/Limine/OVMF RAM-backed boot accepted USB typing;
and the ordinary UEFI image accepted keyboard input and mouse reports through
an emulated USB hub. Seven installer regressions passed, including old-config
upgrade/fallback. Logs are `build/review/usb-fix-*.log`; the validation and
artifact hash record is `build/review/usb-fix-validation.json`. This candidate
has not been installed on the ESP or tested physically; the installed release
is still `1feaf98d0634e58b`. Sudo in this agent session requires authentication.


## Second physical boot: connected USB2 ports never enabled

The user installed and booted `ad2f72b629a8797a`; this supersedes the preceding
candidate status. The supplied photograph shows OS ownership, operational xHCI,
16/16 scratchpads, one successful no-op, one addressed device and zero keyboards.
Ports 1/2 report `0x20ae1`, ports 3/5 `0x206e1`: connected USB2 ports with PED
clear, Polling link state, and a pending connection-status change. Port 14 is
enabled (`0x1203`) and its Apple `05ac:8406` device reaches interface selection.
The expected internal keyboard `05ac:0291` on port 5 is not enumerated. The text
naming that expected keyboard is a reference, not a detection result.

The old code scanned ports once immediately after controller startup; the final
photo was taken later and therefore cannot establish whether CCS was set at
that initial scan. Its reset loop also treated PR=0 alone as completion. The
new candidate powers ports, observes connections over a bounded one-second
window, debounces for 100 ms, clears stale completion flags, and waits for fresh
PRC with PR clear before the 10 ms reset recovery interval. Neutral PORTSC writes
preserve RW-sticky fields without accidentally disabling a port through PED.
Per-port initial/reset-end/current snapshots and named results distinguish a
missed connection, failed reset, and later device enumeration failures. This
addresses concrete discovery/reset defects; physical confirmation is pending.

Host tests now model delayed connection/reset acceptance, stale PRC/PED,
MFINDEX wrap, no connection, reset timeout, disabled completion, disconnect,
stalled clock and an already-enabled SuperSpeed link using the actual driver
code. The installed current release remains `ad2f72b629a8797a`, with
`1feaf98d0634e58b` as previous, until the user runs the installer. No reboot or
privileged installation was performed by this turn.

Candidate `c02ec72ad1c8f3d4` is now staged and its payload hashes were verified.
The kernel build, extended host xHCI regressions, seven installer regressions,
and GRUB → Limine → OVMF RAM boot with `echo grub-usb-ok` typed through USB all
passed. QEMU reports `reset=ready attempts=1`, PRC in the reset-end snapshot,
and a configured keyboard. The validation record is
`build/review/port-reset-validation.json`; logs use the `port-reset-` prefix.
Physical keyboard operation on the Mac remains unverified for this candidate.

## Third physical boot: built-in keyboard confirmed

The user reports that release `c02ec72ad1c8f3d4` booted with working keyboard
input. A subsequent photograph shows `srv> xhci` and the live controller dump.
The internal Apple `05ac:0291` device is port 5 / slot 7, addressed and configured,
with interface 1, DCI 5 and a 64-byte packet size. It is marked `ready`; the live
dump records 27 interrupt completions and 27 HID reports, and port 5 is enabled.
This supersedes the previous pending physical-validation status. The host's
installed state confirms current `c02ec72ad1c8f3d4`, previous `ad2f72b629a8797a`.

The controller reports five addressed devices, four configured, one HID keyboard,
two HID mice, and one hub. The mouse-class devices are on ports 1/2; these counts
do not demonstrate Apple trackpad operation. Apple `05ac:8406` remains at interface
selection. Basic built-in keyboard operation is now the confirmed hardware
baseline. Exhaustive key coverage, repeat boots, and trackpad operation remain
untested. The photo does not isolate which of the combined discovery/reset
corrections was necessary on this controller. No kernel or boot payload changes
were made in response to this successful test.
