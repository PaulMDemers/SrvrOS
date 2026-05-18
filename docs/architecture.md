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
- Userspace `mmap` currently covers eager private mappings owned by the
  process: zero-filled anonymous regions and file-backed regular-fd snapshots.
  `mprotect` can change page permissions, `PROT_NONE` guard pages are modeled by
  clearing user access on present pages, `msync` validates private mmap-owned
  ranges as a no-op, `munmap` frees pages, and process exit cleans up any
  remaining mappings. Demand paging, shared mappings, writeback,
  replacement-style `MAP_FIXED`, and signal-delivered page-fault recovery are
  still future work.
- Kernel heap and static subsystem limits are still intentionally conservative.

## Scheduling And Processes

srvros can run kernel threads and ring-3 processes. The local APIC timer drives
preemption. Process state includes address-space ownership, kernel trap stack,
fd table, GUI queue state, network handle ownership, and exit status. The
process table and VMM address-space tracker currently have 64 slots, and the
scheduler has 32 kernel-thread slots for boot services, foreground processes,
detached jobs, and pipeline stages. Dead scheduler-thread stacks are returned to
the PMM before slot reuse.

The shell supports foreground jobs, foreground/background pipeline groups,
`jobs`, `jobs -l`, `wait`, `fg`/`bg`, `%+`/`%-` job references, and built-in
`kill` for process ids or job references. Sleeping syscalls use wait queues for
keyboard input, network accept/read readiness, and descriptor readiness in
`poll`/`select`, so a blocked process does not busy-spin.

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
- Network: DHCP, status, DNS, listen, accept, connect.
- Console/graphics/input: console info, clear, cursor positioning, a small
  framebuffer-side ANSI CSI subset for common cursor/erase sequences, key scan,
  framebuffer info/pixels/rects, mouse scan.
- Memory mapping: `mmap`, `munmap`, `mprotect`, `msync`.
- GUI IPC: register server, send message, receive message.

Appendable output structs use a small ABI header (`abi_version` and
`struct_size`) at the front of the caller buffer. The userspace wrappers fill
that header before entering the kernel, and the kernel copies back only the
caller-declared size after validating the version. This covers process listing,
file and filesystem status, console/graphics info, mouse events, GUI messages,
and the network enumeration/status/ARP structs.

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
  timestamps when the same AHCI-backed image is mounted after reboot. Updates
  are staged through `/fat/.srvros/meta.tmp`; mount recovery promotes a valid
  temp sidecar or discards a malformed one before applying metadata.

The generated test image reserves multi-cluster directory tables for the root
directory and `/fat/bin`, with fail-fast overflow checks in the image builder so
adding more bundled programs does not silently corrupt directory entries.

Current filesystem caveats:

- Directory rename is intentionally limited to empty directories.
- There is no general journaling or transaction rollback.
- Crash consistency is not guaranteed if QEMU exits during arbitrary mutation,
  though metadata sidecar replacement has a small temp-file recovery path.
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
- Incoming IPv4 checksum validation plus TCP/UDP checksum validation before
  packets are dispatched to protocol state.
- ICMP echo replies.
- UDP for DHCP, DNS, and userspace datagram sockets.
- DHCP address/router/DNS configuration.
- DNS A-record lookup over UDP/53.
- DNS server selection prefers DHCP DNS, then userspace `DNS_SERVER` or
  `/fat/etc/resolv.conf` fallback where applicable, then QEMU's default DNS.
- A compact TCP implementation sufficient for poll-driven static HTTP serving.
- Client-side TCP connect for simple outbound HTTP over QEMU user networking.
- A 32-slot TCP connection table, sized for the current low-traffic web-server
  milestone so short `TIME_WAIT` bursts do not starve outbound client connects.
- TCP table-pressure accounting tracks capacity, current `TIME_WAIT` entries,
  full-table drops, reclaimed `TIME_WAIT` entries, and close timer values in
  the network status ABI.
- Network enumeration/status/ARP structs carry a version and caller-declared
  size prefix. The kernel validates that header and copies back only the bytes
  the caller says it can accept, so future status fields can be appended without
  overwriting older userspace buffers.
- Under table pressure, expired `TIME_WAIT` entries are reaped first; if the
  table is still full, the oldest `TIME_WAIT` slot can be reclaimed so fresh
  web traffic and outbound client connects do not stall behind closed sockets.
- TCP socket lifecycle tracking for process-owned connections, including
  FIN-on-close, kernel-backed read/write `shutdown` state, and compact
  `FIN_WAIT`/`CLOSE_WAIT`/`LAST_ACK`/`TIME_WAIT` cleanup states.
- Per-connection socket error reporting for resets, refused connects,
  timeouts, and in-progress nonblocking connects, exposed to userspace through
  `getsockopt(SO_ERROR)`.
- Segmented TCP writes from userspace buffers larger than one payload-sized
  frame, covered by `webd` serving a multi-kilobyte static response.
- A small per-connection TCP transmit history that retires SYN, FIN, and data
  frames on ACK and retransmits them from the network timer when needed.
- Bounded TCP send backpressure: connection writes only send while there is
  transmit-history/window space, nonblocking writes report `EAGAIN`, and
  `POLLOUT` follows actual send availability.
- Peer-advertised TCP window tracking, so payload sends are bounded by both the
  local outstanding-send limit and the remote receive window advertised in ACKs.
- Dynamic advertised receive windows based on unread connection-buffer space,
  window-update ACKs after userspace reads, and a small zero-window persist
  timer so blocked peers can discover reopened receive windows.
- Idle established-connection cleanup so abandoned TCP clients do not occupy the
  small fixed connection table indefinitely.
- TCP reset responses for closed ports, missing connections, and full
  connection-table SYN attempts, so clients fail promptly instead of waiting for
  retransmission timeouts.
- In-order TCP receive validation: duplicate or out-of-order data/FIN segments
  are acknowledged at the current receive point instead of being appended twice
  or advancing the stream prematurely.
- Userspace IPv4 UDP sockets with `sendto`/`recvfrom`, nonblocking readiness,
  local datagram delivery, and `/fat/bin/udpdns` plus `/fat/bin/udpecho`
  coverage.
- Kernel socket-table enumeration for `/fat/bin/netstat`, covering TCP
  listeners, active TCP connections, UDP sockets, owning PIDs, queue depths,
  send-window availability, peer windows, error state, and compact TCP flags.
- Kernel network status/config reporting for `/fat/bin/ifconfig`,
  `/fat/bin/route`, and `/fat/bin/arp`, including e1000 MAC/IP, DHCP route/DNS
  details, protocol counters, socket counts, worker counters, and the current
  ARP resolution.
- A small fixed ARP cache plus kernel-backed ICMP echo requests for
  `/fat/bin/ping`.
- `/fat/bin/netcheck`, a compact guest-side network health check covering DHCP,
  DNS, ping, UDP loopback, and outbound TCP HTTP.
- `/fat/bin/netabi`, a regression probe for the size-versioned network structs
  that passes deliberately truncated buffers and checks canaries after them.

Network file descriptors are process-owned and cleaned up on process exit.
`webd` listens on port 80, polls the listener plus active connection fds, and
serves files from `/fat/www`. It supports nested static asset paths, GET/HEAD,
content lengths, basic MIME/cache headers, idle partial-client cleanup, and a
small bounded active-client table.

The shell service manager reads `/fat/etc/services/*.svc` files and uses them
to start, stop, restart, and inspect persistent userspace daemons. The generated
exFAT image ships `/fat/etc/services/webd.svc`, and the kernel starts
`/init --system` after mounts/network setup. System init runs
`/fat/etc/init.sh` through a quiet redirected shell and stores startup output
in `/fat/var/log/init.log`; the monitor console receives only concise init
status. The init script launches `svscan &`. `/fat/bin/svscan` continuously
scans enabled service configs, starts missing daemons, and restarts configs
marked `restart=always`, recording supervisor events in
`/fat/var/log/svscan.log`. Service configs can declare `requires=network`,
`health=listen:<port>`, and `max_log=<bytes>`; `svscan` waits for declared
dependencies, restarts unhealthy listeners, and rotates oversized daemon logs.
Service stdout is redirected to live append logs such as
`/fat/var/log/webd.log`, and `service list` reports process state with
enabled/restart/dependency/health/log metadata. `service enable <name>` and
`service disable <name>` rewrite the service config, and `service reload`
touches `/fat/run/svscan.reload` so `svscan` logs and consumes an explicit
rescan request. `service <name> log`, `service <name> tail [lines]`,
`service <name> check`, `service <name> check-config`, and
`service <name> rotate-log` cover basic operations, while
`service set <name> <key> <value>` and `service unset <name> <key>` edit
configs in-place. `service restart <name> --wait` asks the supervisor to bring
the service back and waits for the declared health check. `service supervise
[cycles]` keeps the same restart policy available as a bounded diagnostic loop.
`svscan` logs start counts and stop statuses for restart forensics. `webd` adds
compact access lines containing method, URL, status, and body bytes.

Service files are simple `key=value` records. Supported keys are `command`,
`args`, `process`, `log`, `requires`, `health`, `max_log`, `enabled`, and
`restart`. `requires=network` waits for the network stack, `health=listen:80`
checks for a TCP listener owned by the service process, `max_log` bounds daemon
log size, and `restart` currently accepts `always` or `never`.

Current networking caveats:

- TCP is intentionally minimal and not a general-purpose implementation yet.
- TCP congestion control, window scaling, SACK, delayed ACK tuning,
  out-of-order receive queues, and full TIME_WAIT/state-machine handling are
  still future work.
  Current close states are deliberately compact and timer-reaped rather than a
  complete RFC-level lifecycle.
- No IPv6.
- UDP sockets are IPv4-only and use a small bounded receive queue.
- DNS currently resolves A records only.

## Userspace

Each userspace program is a static freestanding ELF linked with the small srvros
support library and shared `userspace/lib/crt0.S` startup object. There is no
external libc dependency, and adding a normal app no longer needs a copied
per-directory `start.S`.

Core tools:

- `sh`, `ls`, `cat`, `more`, `echo`, `write`, `wc`, `clear`, `ps`, `kill`.
- `grep`, `head`, `stat`, `chmod`, `cp`, `rm`, `mkdir`, `mktemp`, `mv`.
- `which`, `env`, `pwd`, `true`, `false`.
- `sleep`, `date`, `touch`, `basename`, `dirname`.
- `tail`, `tee`, `find`, `du`, `df`, `sort`, `uniq`, `cut`, `xargs`, `seq`,
  `realpath`, `id`, `whoami`, `readlink`, `cmp`, `yes`, `install`, `diff`,
  `tar`, `gzip`, `gunzip`, `minizip`, `miniunz`, `patch`, `make`, `sed`, `expr`, `printf`, `tr`,
  `uname`, `hostname`, `uptime`.
- `webd`, `httpget`, `udpdns`, `udpecho`, `netstat`, `ifconfig`, `route`,
  `arp`, `ping`, `host`, `spin`, `ui`, `desktop`, `calcgui`, `notesgui`, `textedit`,
  `imgedit`.

The shell has PATH lookup for `/fat/bin` and `/`, sourceable scripts,
non-interactive `sh -c command` and `sh script` modes, stdin/stdout/stderr
redirection, multi-stage pipelines, foreground/background jobs, `$VAR`/`${VAR}`
including default/assign/alternate/error parameter forms, `${#VAR}`,
prefix/suffix trims, `$?`/`$$`/`$!`, positional parameters (`$0`, `$1`, `$#`,
`$@`), `$(command)` command substitution, unquoted `*`/`?` globbing, unmatched quote/block
diagnostics, Ctrl-C prompt recovery, `&&`/`||`, compound-command tail chaining,
command-local `NAME=value`, comments, script line continuations, simple
here-docs, current-shell `{ ...; }` grouping,
`if`/`then`/`else`/`fi`, `for`/`in`/`do`/`done`, `while`/`do`/`done`,
`case`/`in`/`esac`,
shell functions with `return`, `shift`, loop `break`/`continue`,
login profile loading from `/fat/etc/profile` plus immediate
`/fat/etc/profile.d/*.sh` snippets, `PS1` prompt expansion for `\w`,
`test`/`[`, `set -e`/`set +e`, `read`, `alias`, `type`, `command`,
`unset`, `TMPDIR`,
`cd -` with directory validation, `jobs`/`jobs -l`/`wait`/`fg`/`bg`/`kill`,
`%+`/`%-` job references, config-backed `service` commands,
DHCP/status/DNS builtins, `env`/`export`/`which`, `env NAME=value command`,
`exec`, and basic filesystem
builtins. Interactive `srvsh` uses the srvros linenoise adapter for editable
input, persistent `/fat/.srvsh_history`, and tab completion for builtins,
aliases, functions, PATH commands, and filesystem paths.

The generated exFAT image also ships `/fat/share/help/*.txt` topic files for
the shell, services, networking, files, web serving, CLI conventions, profiles,
and the pager. The `help` builtin prints a compact summary, `help -l` lists
topics, `help <topic>` and `man <topic>` read those files directly from the
mounted filesystem, and `apropos <word>` searches topic names and content.
`/fat/bin/more` provides a small page-at-a-time reader with `--plain` for
non-interactive scripts. The image also includes `/fat/share/examples`,
`/fat/tmp`, and `/fat/home` so scripts have predictable defaults on first boot.
The table-stakes command set follows the same short-help convention: `-h` and
`--help` print usage and exit successfully, and common file/text tools accept
`--` to stop option parsing.

Userspace path handling now canonicalizes relative paths against the inherited
`PWD`, including repeated separators plus `.` and `..` components. The syscall
wrapper layer and POSIX path helpers share the same normalization behavior, so
shell commands, libc callers, and spawned processes see consistent paths.

The terminal signal path handles Ctrl-C from serial or PS/2 keyboard input. The
kernel routes it to the active foreground process group, including every member
of a shell pipeline, instead of the root interactive shell. It marks matching
processes for `SIGINT`, wakes blocking pipe/poll/network I/O where possible,
and exits them with the conventional `128 + signal` status. `kill` uses the
same process signal machinery with `SIGTERM`.

The first text-tool compatibility passes cover common script-facing flags:
`grep -i/-n/-v/-c/-q`, `wc -l/-w/-c`, compact `head -1`/`tail -1` aliases for
line counts, `find -type f|d`, `ls -a/-l` with multiple paths, and a literal
`sed` subset with `-n`, `-e`, `s`, `p`, `d`, line-number addresses, and
`/pattern/` addresses. Shell `test`/`[` covers boolean `-a`/`-o`/`!`, common
file probes, mtime comparisons, and same-file checks. `/fat/bin/seq` covers
integer ranges, `id`/`whoami` provide a fixed root identity, `cmp` compares
files, `yes` feeds simple pipeline prompts, `xargs` can batch with `-n` and skip
empty input with `-r`, `install` copies files into build-style destination trees,
`diff` reports quiet or simple unified differences, `tar` can create/list/extract
uncompressed ustar archives, `gzip`/`gunzip` handle gzip-framed streams backed
by the pinned zlib port, `minizip`/`miniunz` cover zip archives, `patch`
applies simple unified diffs, `make` runs small dependency/recipe graphs
through the shell, `byacc` generates C parsers from small yacc grammars, and
`realpath` plus
`readlink -f` normalize checked paths for porting scripts.
`/fat/bin/expr` covers simple integer arithmetic,
comparisons, string length/substr/index, and literal-prefix `:` matching for
script glue. `/fat/bin/printf` supports common `%s`/numeric formatting and
escapes; `/fat/bin/tr` supports simple set translation, ranges, and deletion.

The current `date` tool reports monotonic uptime because srvros does not yet
have RTC or network time plumbing.

New launches use the native `SYS_EXEC` request path. It copies a path, argv
vector, envp vector, background flag, optional standard fd overrides, and an
in-place replacement flag from userspace. Spawn requests build a fresh child
ring-3 stack; replacement requests first load the new image into a temporary
kernel-owned process image, then swap the current process to the new address
space and enter the new program with the same pid while closing any fds marked
close-on-exec. The POSIX
compatibility layer maps this to `posix_spawn`, `posix_spawnp`, `waitpid`,
standard-fd dup/open/close spawn file actions, ordered non-stdio spawn file
actions, `POSIX_SPAWN_SETPGROUP`, and process-replacing `execve`.

## POSIX Compatibility

The first POSIX-compat layer is implemented in userspace on top of srvros
syscalls. It exposes common headers such as `unistd.h`, `fcntl.h`, `errno.h`,
`dirent.h`, `pthread.h`, `sched.h`, `sys/stat.h`, `sys/ioctl.h`,
`sys/socket.h`, `netdb.h`, and `time.h`.

This layer currently covers basic file I/O, `O_RDWR` regular-file descriptors,
`stat`/`fstat` with VFS-managed metadata, `chmod`/`fchmod`, `umask`,
`dup`/`dup2` for standard streams, pipes, writable regular files, and
read-only regular files, `poll`/`select` readiness, blocking pipes,
`O_NONBLOCK`/`fcntl` status flags, `F_GETFD`/`F_SETFD` descriptor flags,
`FD_CLOEXEC`, permission-aware `access`, `isatty`, `fsync`,
`truncate`/`ftruncate`, `statvfs`, minimal terminal `tcgetattr`/`tcsetattr`
plus `ioctl` window-size queries, directory iteration, path/cwd state, `sbrk`-backed
malloc-family allocation, kernel-backed `brk`/`sbrk`, anonymous and
file-backed private `mmap`/`munmap`, `mprotect`, small buffered `stdio`,
simple time functions including `nanosleep`, `getpagesize`/`sysconf`,
same-address-space `pthread_create`/`pthread_join`/`pthread_detach`,
pthread mutex/condition/once/TLS primitives, common
formatted-output width/precision/flag forms,
`scanf`/`sscanf` basics including scansets, `system()` via shell spawn,
`popen`/`pclose`, `getpid`, `waitpid`, `posix_spawn`, `posix_spawnp`,
standard-fd and ordered non-stdio spawn file actions,
`POSIX_SPAWN_SETPGROUP`, process-replacing `execve`, IPv4 formatting and
parsing, userspace UDP DNS-backed `getaddrinfo`, a TCP server socket flow mapped
onto srvros listener/connection fds, client-side `connect`, and socket name
queries. The kernel additions
for this slice are intentionally narrow: fd metadata/duplication, shared regular-file open
descriptions, fd readiness checks, nonblocking read/accept/write returns, child
stdio fd overrides plus inherited parent stdio redirects, seek, fd
flush/truncate, process heap growth, `getpid`, raw timer ticks,
sleep-by-ticks syscalls, a shared fd readiness wait queue,
timer-backed scheduler wait deadlines, and runtime VFS inode/mode/timestamp
metadata with exFAT sidecar persistence.

User pthreads are backed by a compact native thread syscall set. The libc shim
allocates a stack with `mmap`, enters the requested routine in the same process
address space, and `pthread_join` collects the returned pointer-sized value.
Each spawned user thread has its own scheduler kernel trap stack and user FPU
state while sharing the owning process fd table, mappings, and heap. Detached
pthread stacks are tracked by libc and reclaimed through a hidden join after
`SYS_THREAD_STATUS` reports completion. If one user thread exits the whole
process, the kernel marks the process exiting, wakes blocked file/pipe/network
waiters, and retires sibling user-thread scheduler contexts before address-space
teardown. `SYS_FUTEX_WAIT`/`SYS_FUTEX_WAKE` provide the first process-local
address-keyed wait primitive; pthread mutexes and condition variables use it to
sleep on shared userspace words instead of spin-yielding. The userspace heap
allocator also uses a process-local futex-backed lock, and `pthread_once` now
tracks unstarted/running/complete states so contending threads wait for the
initializer to finish before returning. The scheduler updates the TSS from each
thread's effective kernel trap stack when switching between user threads, and
the native thread launch path preserves the exact userspace stack alignment
chosen by libc so SSE-using code keeps the SysV ABI alignment it expects.
Regular-file `read`/`write`/`seek` offset updates are serialized in the kernel
under preemption, and libc stdio streams have recursive futex-backed locks for
shared `FILE *` use from pthreads. `/fat/bin/threadstress` plus
`tools/thread_smoke.py` cover the current threading contract in QEMU.

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
