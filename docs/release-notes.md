# Release Notes

## Initial Repository Milestone

This milestone captures srvros as a bootable x86_64 research OS with a working
kernel, minimal userspace, filesystem mutation, networking, and a background web
server.

### Highlights

- Boots a higher-half x86_64 kernel through Limine.
- Runs freestanding ring-3 ELF programs from initramfs and `/fat`.
- Schedules kernel threads and userspace processes with timer preemption.
- Provides foreground/background process control through the monitor and shell.
- Mounts exFAT from initramfs-backed memory or AHCI-backed disks.
- Supports exFAT file create/write/append/delete/rename, directory create,
  empty directory removal, mount/unmount, and consistency checks.
- Drives an Intel e1000 NIC in QEMU with interrupt-backed receive handling.
- Supports ARP, ICMP echo, DHCP, DNS A-record resolution, and enough TCP for a
  userspace HTTP server.
- Ships `/fat/bin/webd`, a ring-3 web server serving static files from
  `/fat/www`.
- Includes a small shell, CLI utilities, service control, redirection, scripts,
  PATH lookup, and background jobs.
- Adds the first POSIX-compat userspace layer for file, directory, errno,
  malloc, time, cwd, IPv4, DNS, and TCP server socket APIs.
- Adds minimal `stdio`, stages zlib and Lua as pinned submodules under
  `ports/upstream`, ships `/fat/bin/zlibdemo`, and adds `/fat/bin/lua` as an
  integer-profile Lua 5.4.8 interpreter.
- Includes early GUI/windowing experiments and sample GUI apps.
- Includes QEMU smoke tests for CLI, processes, directories, DHCP, DNS, web
  serving, and filesystem stress.

### Screenshots

![srvros boot console](assets/screenshots/console-boot-monitor.png)

![srvros desktop with GUI apps](assets/screenshots/desktop-apps.png)

### Verified Commands

The latest release prep pass verified:

```sh
make -j4
python3 tools/dir_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dns_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64 --line-wait 12
python3 tools/cli_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dhcp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/web_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/ports_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/lua_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/fs_stress.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 1 --line-wait 3
```

### Known Limits

- TCP is intentionally small and aimed at the current web-server milestone.
- DNS resolves A records only.
- exFAT mutation has no journaling or crash-safe transaction model.
- Empty directory rename/removal is supported; non-empty directory removal is
  rejected.
- Device support is oriented around QEMU q35, AHCI, e1000, PS/2, serial, and a
  linear framebuffer.
- The GUI stack is a prototype and not yet a general application ABI.
- `stdio` is deliberately small: enough for early command-line ports, not a full
  ISO C implementation.
- Lua currently uses integer numbers and excludes `math`, `io`, `os`,
  `package`, and dynamic loading. Full Lua floating numbers need kernel FPU/SSE
  context support.
- Repeated large Lua process launches in one boot still need kernel heap/process
  teardown hardening; the release smoke covers one script execution per fresh
  boot.

### Next Release Themes

- Harden exFAT writes with sync, rollback, and broader fragmented-chain testing.
- Add richer userspace networking APIs and readiness primitives.
- Add UDP sockets and stronger DNS resolver behavior.
- Add NVMe storage.
- Expand the shell and support library toward a small libc-shaped environment.
- Move GUI windows toward client-owned shared framebuffers.
