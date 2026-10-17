# USB storage groundwork for automated laptop tests

The target is the attached Phison `13fe:6700` flash drive: USB 3, SCSI
transparent/Bulk-Only Transport (BOT), 1024-byte bulk packets, burst 3,
MBR primary partition 1 starting at LBA 2048, exFAT UUID `37D8-C5D0`.
Linux's `/dev/sdc` name is only an observation, never a write target in these
tools. The internal SATA SSD remains disabled in the hardware boot profile.

## Implementation

- xHCI selects a complete alternate-zero BOT interface and configures its bulk
  IN/OUT endpoints. SuperSpeed endpoint companions supply burst sizes.
- A separate BOT/SCSI driver validates command/status signatures, tags,
  completion lengths, residues and status. It probes LUN 0, handles bounded Unit
  Attention responses, ignores empty readers, reads/writes sectors, and issues
  SYNCHRONIZE CACHE on flush. Supported sector sizes are 512–4096 bytes; capacity
  must fit READ/WRITE(10). A failed command reports failure to the block layer.
- Malformed responses, phase errors, stalls and timeouts quarantine that device
  until reboot. No ambiguous write is retried. The controller's DMA buffers and
  rings remain allocated so late transfers cannot corrupt reused memory. Runtime
  bulk waits have a two-second reference-clock deadline plus an iteration cap.
- Storage request ownership covers buffers, partial-sector updates and flushes.
  Keyboard and console services may run during waits; other storage callers are
  excluded. Early boot works before scheduler service windows are enabled.
- Every event-ring consumer preserves HID/bulk completions for their recipient.
  Matching includes slot, endpoint and TRB address. Event processing is IRQ
  serialized, and the keyboard service has a bounded event batch.
- Primary MBR discovery creates `usb0p1`-style aliases. Invalid bounds, overlap,
  protective/hybrid layouts and nested discovery are rejected; extended
  containers are not exposed as volumes. Discovery is idempotent.
- USB disks register read-only by default. `srvros.usb-storage=rw` enables writes
  explicitly; this option is used only in disposable QEMU tests for this batch.
  USB insertion does not change the `/fat` RAM root or redirect boot logs.
- QEMU exposed an existing scheduler stack-alignment defect. New kernel threads
  entered a C function with RSP aligned as a caller, rather than a callee. The
  scheduler now reserves the ABI return-address slot. This fixes the aligned
  SIMD store fault exposed by the updated keyboard poller.

No physical flash-drive contents, partition tables, EFI files or GRUB state
were changed by this implementation/test batch. The kernel has not yet been
validated against the physical flash drive.

## Candidate

The staged release is `ae227d9b6bda1014`, with kernel SHA-256
`afb8e63aa468d4ea15ea0768eff1b73f747b710b43732f9d34eb9e97eb6d409a`.
Its verified manifest records the source checkout as dirty; it is an immutable
snapshot of this batch, not a claim that the changes are committed. The installed
current release is still `70ddde650d0fc3f8`, previous `3cd2e11a98996ccb`.

## Tests

Verified results for the staged candidate:

| Check | Result |
| --- | --- |
| Host C regression groups | 14 passed |
| Installer tests | 8 passed |
| USB read-only boot | Mount, fsck, file comparison, write rejection; whole-disk hash unchanged |
| USB writable boot | 64 KiB throttled write with queued keyboard input; log write, sync, fsck |
| USB fresh-process reload | Saved log read back; read-only disk hash unchanged |
| USB stalled read | Quarantined within bounded wait; keyboard and monitor remained usable |
| Interrupt/FPU smoke | Nested traps, timer preemption and userspace FP passed |
| Syscall ABI smoke | Passed |
| Userspace CLI regression | All 479 commands passed, including POSIX signal/thread probe; final fsck clean |
| Staged GRUB/OVMF boot | Kernel, console and responsiveness checks passed |

Run from the repository root:

```sh
tools/host-env.sh make -j2 all hardware-stage test-core
tools/host-env.sh python3 tools/usb_storage_smoke.py
tools/host-env.sh python3 tools/usb_storage_smoke.py --timeout-only
```

The USB smoke harness attaches a **USB mass-storage device** to QEMU xHCI,
with a disposable MBR disk containing a copy of `build/srvros.exfat`. Its boot
profile disables AHCI. It verifies mounting, filesystem checks, executable
comparison, read-only write rejection and disk hashes, throttled writes with
concurrent USB keyboard input, explicit sync, and persistence across a new
QEMU process. The timeout mode stalls a read and checks quarantine plus keyboard
progress. It never opens a host block device.

Host tests use the captured flash-drive configuration descriptor (no strings or
serial number), both xHCI context sizes, malformed endpoint descriptions,
interleaved keyboard/bulk completions, timeout quarantine, BOT protocol errors,
empty media, Unit Attention, unaligned sector I/O, ownership, MBR map rejection
and the new-thread ABI stack layout.

Evidence is under `build/review/usb-storage-*.log`, `build/usb-smoke.log`,
`build/usb-final-build.log`, and the interrupt/syscall/CLI regression logs under
`build/review/usb-*-smoke.log`. Build outputs are local, not source-controlled.

## First laptop check

Leave the flash drive plugged in before boot; runtime hotplug is not implemented.
Install the staged candidate and request a one-shot boot when ready:

```sh
sudo python3 tools/hardware_boot.py install
sudo grub-reboot srvros-current
sudo systemctl reboot -i
```

At the SrvrOS monitor:

```text
block
mount usb0p1 /usb
ls /usb
mount
```

`block` should show a roughly 62-billion-byte USB disk and its partition.
The mount listing should report `/usb` as non-writable. If another populated
USB disk is connected, use the corresponding `usbNp1` name from `block`;
these names are assigned at discovery and do not correspond to Linux names.
The built-in empty Apple card reader should not claim a disk name. An empty
flash drive can legitimately produce an empty directory listing.

If mounting fails, photograph `block` and `xhci`. The latter now includes a
storage line with stage, disk name, endpoint sizes/bursts, command/error counts
and offline status. No format or repair operation is needed.

## Automation follow-up

The user subsequently confirmed that release `ae227d9b6bda1014` mounted the
physical flash drive. The [automatic hardware test cycle](automated-hardware-tests.md)
now implements volume selection, single-use requests, test/report writes and
flushes, ACPI reset, one-shot GRUB return and validated result collection.
Its end-to-end QEMU and failure tests pass; physical write persistence and reset
are the next laptop check. Hard hangs still require separate watchdog work.

The current driver intentionally omits UAS, multiple LUNs, runtime hotplug,
transport reset recovery, capacities needing READ CAPACITY(16), and USB GPT or
extended-partition discovery. These limits do not conflict with the captured
flash-drive layout. SuperSpeed hub enumeration and general composite-device
support are also outside this change.

Protocol references: [USB-IF Bulk-Only Transport 1.0](https://www.usb.org/sites/default/files/usbmassbulk_10.pdf)
and [Intel xHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf).
