# srvros Architecture Notes

This document describes the current srvros architecture at the initial public
repo milestone. The system is intentionally small and direct: most subsystems are
single-purpose, static-capacity, and easy to inspect.

## Boot Flow

1. Limine loads the higher-half ELF kernel and `build/initramfs.tar`.
2. The kernel initializes serial logging, the framebuffer console, GDT, IDT,
   memory management, scheduler structures, ACPI/MADT, APIC/IOAPIC routing,
   PS/2 devices, PCI, block devices, AHCI, exFAT, e1000, and the monitor.
3. The initramfs is mounted through VFS and also carries `srvros.exfat`.
4. If an AHCI disk with the generated exFAT image is present, it becomes `/fat`.
   Otherwise `/fat` falls back to a memory-backed block device over the image
   embedded in initramfs.
5. The monitor waits at `srv>` and can launch ring-3 ELF programs from VFS.

## Kernel Shape

The kernel is freestanding C with small x86_64 assembly entry points for
interrupts, syscalls, usermode entry, and context switching.

Important pieces:

- `kernel/src/main.c`: bootstrap sequence.
- `kernel/src/arch/x86_64`: descriptor tables, interrupts, APIC, IOAPIC, PCI,
  serial, keyboard, mouse, and syscall dispatch.
- `kernel/src/memory`: physical frames, heap, and virtual memory mapping.
- `kernel/src/process.c`: process table, ELF loading, fd ownership, cleanup.
- `kernel/src/scheduler.c`: preemptive scheduler and wait queues.
- `kernel/src/vfs.c`: stable VFS node registry.
- `kernel/src/block.c`: block-device registry and write-through cache.
- `kernel/src/fs/exfat.c`: exFAT mount/read/write/directory operations.
- `kernel/src/net.c`: e1000-facing ARP, ICMP, DHCP, DNS, TCP, and fd handoff.
- `kernel/src/gui.c`: fixed-size GUI IPC queues for the desktop experiment.

## Memory Management

Physical memory is discovered from Limine's memory map. The PMM tracks frames
with a bitmap and supports single/contiguous allocation. The kernel heap is a
simple allocator used by VFS, process, networking, and filesystem code.

The kernel runs higher-half. Processes get separate page tables with user pages
for ELF text/data/stack plus propagated kernel mappings. Syscalls validate user
buffers and strings against present user-accessible mappings before copying.

Current tradeoffs:

- There is no demand paging.
- There is no userspace `mmap`.
- Kernel heap and static subsystem limits are still intentionally conservative.

## Scheduling And Processes

srvros can run kernel threads and ring-3 processes. The local APIC timer drives
preemption. Process state includes address-space ownership, kernel trap stack,
fd table, GUI queue state, network handle ownership, and exit status.

The shell supports foreground jobs, background jobs, `jobs`, `wait`, and `kill`.
Sleeping syscalls use wait queues for keyboard input, network accept/read
readiness, and descriptor readiness in `poll`/`select`, so a blocked process
does not busy-spin.

## Syscall ABI

Userspace calls the kernel through `int 0x80`. Shared syscall numbers live in
`shared/include/srvros/syscall_numbers.h`; userspace wrappers live under
`userspace/lib/src`.

The ABI currently covers:

- Basic fd I/O: `open`, `open_mode`, `read`, `write`, `seek`, `close`.
- Filesystem mutation: `fs_write`, `fs_append`, `unlink`, `mkdir`, `rmdir`,
  `rename`, `stat`, `list`.
- Process control: `spawn`, `spawn_args`, `spawn_bg`, redirected spawn, process
  list, kill, wait, yield, exit.
- Network: DHCP, status, DNS, listen, accept.
- Console/graphics/input: console info, clear, cursor positioning, a small
  framebuffer-side ANSI CSI subset for common cursor/erase sequences, key scan,
  framebuffer info/pixels/rects, mouse scan.
- GUI IPC: register server, send message, receive message.

## VFS And Filesystems

The VFS is a stable node registry keyed by path. Filesystems register nodes with
read callbacks and optional release hooks. Stable slots keep open file pointers
valid while unrelated mounts are deregistered.

exFAT is the primary filesystem. It supports:

- Mount from memory block device or writable AHCI block device.
- Recursive directory scanning.
- Root and nested file reads.
- File create, overwrite, append, delete, rename.
- Directory create and empty directory removal.
- Empty directory rename.
- Runtime mount/unmount with busy checks.
- Allocation bitmap and FAT-chain awareness.
- A consistency checker exposed as `fsck /fat`.
- VFS-level Unix-like metadata: inode ids, mode bits, uid/gid placeholders,
  block counts, and tick-derived access/modify/change timestamps.
- A srvros-managed metadata sidecar at `/fat/.srvros/meta` on writable exFAT
  mounts. The sidecar restores inode ids, modes, uid/gid placeholders, and
  timestamps when the same AHCI-backed image is mounted after reboot.

The generated test image reserves multi-cluster directory tables for the root
directory and `/fat/bin`, with fail-fast overflow checks in the image builder so
adding more bundled programs does not silently corrupt directory entries.

Current filesystem caveats:

- Directory rename is intentionally limited to empty directories.
- There is no journaling or transaction rollback.
- Crash consistency is not guaranteed if QEMU exits during mutation.
- Long-name and fragmented-chain support is practical but still being hardened.
- Unix-like metadata is persisted by srvros in a sidecar file rather than in
  native exFAT directory entries.

## Block And Storage

`kernel/src/block.c` provides a generic block-device table and a small
write-through cache. Devices expose byte-offset read/write callbacks and fixed
block geometry.

Implemented devices:

- Memory-backed initramfs exFAT image.
- AHCI SATA disks using IDENTIFY, READ DMA EXT, and WRITE DMA EXT.

The Makefile can boot with one or two AHCI-attached exFAT images. This lets the
same filesystem code run against both an embedded image and a real block path.

## Networking

The e1000 driver configures descriptor rings and uses IOAPIC-routed interrupts.
The IRQ path acknowledges device causes and schedules bottom-half work so packet
draining happens outside interrupt context. A low-rate timer wake prevents a lost
interrupt from stranding RX frames.

The network stack includes:

- Ethernet frame parsing.
- ARP replies and ARP lookup for outbound traffic.
- IPv4 packet handling.
- ICMP echo replies.
- UDP for DHCP and DNS.
- DHCP address/router/DNS configuration.
- DNS A-record lookup over UDP/53.
- A compact TCP implementation sufficient for poll-driven static HTTP serving.

Network file descriptors are process-owned and cleaned up on process exit.
`webd` listens on port 80, polls the listener plus active connection fds, and
serves files from `/fat/www`. It supports nested static asset paths, GET/HEAD,
content lengths, basic MIME/cache headers, idle partial-client cleanup, and a
small bounded active-client table.

Current networking caveats:

- TCP is intentionally minimal and not a general-purpose implementation yet.
- No IPv6.
- No UDP userspace sockets yet.
- DNS currently resolves A records only.

## Userspace

Each userspace program is a static freestanding ELF linked with the small srvros
support library and shared `userspace/lib/crt0.S` startup object. There is no
external libc dependency, and adding a normal app no longer needs a copied
per-directory `start.S`.

Core tools:

- `sh`, `ls`, `cat`, `echo`, `write`, `wc`, `clear`, `ps`, `kill`.
- `grep`, `head`, `stat`, `chmod`, `cp`, `rm`, `mkdir`, `mv`.
- `which`, `env`, `pwd`, `true`, `false`.
- `webd`, `spin`, `ui`, `desktop`, `calcgui`, `notesgui`, `textedit`,
  `imgedit`.

The shell has PATH lookup for `/fat/bin` and `/`, scripts, stdin/stdout/stderr
redirection, multi-stage pipelines, foreground/background jobs, `$VAR`/`${VAR}`,
`$?`/`$$`, unquoted `*`/`?` globbing, `&&`/`||`, `test`/`[`, `service webd`,
DHCP/status/DNS builtins, `env`/`export`/`which`, `exec`, and basic filesystem
builtins.

New launches use the native `SYS_EXEC` request path. It copies a path, argv
vector, envp vector, background flag, optional standard fd overrides, and an
in-place replacement flag from userspace. Spawn requests build a fresh child
ring-3 stack; replacement requests first load the new image into a temporary
kernel-owned process image, then swap the current process to the new address
space and enter the new program with the same pid while closing any fds marked
close-on-exec. The POSIX
compatibility layer maps this to `posix_spawn`, `posix_spawnp`, `waitpid`, basic
stdio fd remapping through spawn file actions, and process-replacing `execve`.

## POSIX Compatibility

The first POSIX-compat layer is implemented in userspace on top of srvros
syscalls. It exposes common headers such as `unistd.h`, `fcntl.h`, `errno.h`,
`dirent.h`, `sys/stat.h`, `sys/ioctl.h`, `sys/socket.h`, `netdb.h`, and
`time.h`.

This layer currently covers basic file I/O, `O_RDWR` regular-file descriptors,
`stat`/`fstat` with VFS-managed metadata, `chmod`/`fchmod`, `umask`,
`dup`/`dup2` for standard streams, pipes, writable regular files, and
read-only regular files, `poll`/`select` readiness, blocking pipes,
`O_NONBLOCK`/`fcntl` status flags, `F_GETFD`/`F_SETFD` descriptor flags,
`FD_CLOEXEC`, permission-aware `access`, `isatty`, `fsync`,
`truncate`/`ftruncate`, minimal terminal `tcgetattr`/`tcsetattr` plus
`ioctl` window-size queries, directory iteration, path/cwd state, `sbrk`-backed
malloc-family allocation, kernel-backed `brk`/`sbrk`, small `stdio`, simple
time functions, `scanf`/`sscanf` basics, `getpid`, `waitpid`, `posix_spawn`,
`posix_spawnp`, process-replacing `execve`, IPv4 formatting and parsing, DNS-backed
`getaddrinfo`, and a TCP server socket flow mapped onto srvros listener/
connection fds. The kernel additions for this slice are
intentionally narrow: fd metadata/duplication, shared regular-file open
descriptions, fd readiness checks, nonblocking read/accept/write returns, child
stdio fd overrides, seek, fd flush/truncate, process heap growth, `getpid`, raw
timer ticks, sleep-by-ticks syscalls, a shared fd readiness wait queue,
timer-backed scheduler wait deadlines, and runtime VFS inode/mode/timestamp
metadata with exFAT sidecar persistence.

The native executable format remains static ELF64. Common Makefile rules link
each program with the shared crt startup object, keeping each app as a single
self-contained executable while leaving room for a userspace `.srvapp` bundle
format later.

The support library also exports a first newlib-facing syscall layer (`_open`,
`_read`, `_write`, `_lseek`, `_fstat`, `_sbrk`, and friends). Existing srvros
apps still link the local libc-shaped implementation, but the ABI now has the
core hooks a real libc port expects.

The compatibility boundary is intentionally in userspace. Unsupported POSIX
features return `ENOSYS` or a narrow error instead of expanding the kernel ABI
prematurely.

## GUI

The GUI layer is experimental and deliberately simple. `desktop` acts as a
fullscreen userspace window server. GUI clients are separate ring-3 processes
that send fixed-size messages for window creation, labels, buttons, text
updates, and events.

The userspace UI library provides buffered elements, parent/child composition,
dirty marking, mouse hit testing, keyboard events, cursor refresh, and basic
controls. This is enough for calculator, notes, text editor, and BMP image
editor experiments.

## Testing Strategy

The test harnesses boot QEMU, connect to the serial console, run monitor/shell
commands, and detect fatal exceptions. They exercise real kernel paths rather
than mocking subsystems. See `docs/testing.md`.

## Near-Term Architecture Goals

- Strengthen exFAT mutation with better rollback and sync semantics.
- Add a userspace socket API and readiness model.
- Add UDP sockets and richer DNS resolver behavior.
- Add NVMe as a second storage backend.
- Grow the support library toward a small libc-shaped layer.
- Move GUI clients toward shared pixel buffers and damage rectangles.
- Add a FUSE-like userspace filesystem interface once fd passing and server
  process supervision are stronger.
