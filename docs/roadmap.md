# srvros Roadmap

srvros is starting as a small x86_64 OS with a Limine boot path, a higher-half
kernel, and a minimal Unix-like userspace.

## Current milestone

- Build with MSYS2 UCRT64 make/QEMU plus a downloaded Zig toolchain for
  freestanding C and LLD.
- Boot with Limine from an ISO in QEMU.
- Enter a higher-half x86_64 kernel.
- Log boot information through COM1 serial.
- Paint the framebuffer so graphical handoff is visible.
- Print the Limine memory map.
- Load a kernel GDT.
- Load an IDT for CPU exceptions.
- Route x86_64 exception stubs into C diagnostics.
- Build a physical frame allocator from the Limine memory map.
- Add contiguous frame allocation.
- Add a small kernel heap.
- Add active page-table walking and single-page mapping helpers.
- Remap and mask the legacy PIC.
- Map the local APIC and use its timer for boot-time IRQ testing.
- Add a PS/2 set-1 keyboard IRQ handler.
- Add a framebuffer text console with built-in glyphs, cursor, backspace, and scrolling.
- Parse ACPI MADT enough to discover the IOAPIC.
- Route PS/2 keyboard IRQ1 through the IOAPIC.
- Add a keyboard ring buffer and interactive `srv>` kernel monitor.
- Build and load a ustar initramfs as a Limine module.
- Add `ls` and `cat` monitor commands backed by initramfs files.
- Add a minimal VFS interface over initramfs nodes.
- Add an ELF64 header probe command for files reachable through VFS.
- Build a freestanding ELF64 `/init` and overlay it into the generated initramfs.
- Add ring-3 GDT/TSS setup and an `iretq` user entry path.
- Add an `int 0x80` syscall gate with minimal `write` and `exit` syscalls.
- Add a monitor `run /path` command that maps and enters a user ELF.
- Generalize VFS nodes with filesystem-specific read callbacks.
- Generate and mount a read-only exFAT image at `/fat`.
- Parse exFAT boot sector, root directory file entry sets, UTF-16 file-name
  entries, contiguous file extents, and FAT chains for file reads.
- Add userspace file syscalls for `open`, `read`, `close`, and VFS listing.
- Replace the one-shot `/init` demo with a ring-3 shell supporting `help`, `ls`,
  `cat`, and `exit`.
- Add a kernel context save/restore path so userspace `exit` returns to the
  kernel monitor.
- Track and free user page mappings when a process exits.
- Add a `spawn(path)` syscall and shell `run` command for launching child ELF
  applications from VFS.
- Add `/fat/bin-hello`, the first separate userspace application launched by
  `srvsh`.
- Move file descriptor state into each process object.
- Add `/fat/bin-cat`, a separate userspace application that opens and reads
  `/fat/notes` through its own fd table.
- Add cooperative kernel threads with context switching and a demo background
  worker.
- Yield to kernel workers while keyboard reads wait for input.
- Add PCI config-space enumeration and a monitor `pci` command.
- Add QEMU `run-net` and `run-virtio-net` targets.
- Add an initial Intel e1000 binding that enables PCI memory/bus-mastering,
  maps BAR0 MMIO, and reads STATUS/MAC registers.
- Add e1000 RX/TX descriptor rings and poll-mode packet movement.
- Add Ethernet frame plumbing with ARP, IPv4, ICMP echo replies, and a small TCP
  subset for QEMU `hostfwd=tcp::8080-:80`.
- Add tagged network handles for TCP request/response handoff to userspace.
- Track per-process ownership for network handles and clean them up during
  process exit.
- Promote listener and connection handles into process file descriptors, so
  accepted connections can use `read`, `write`, and `close`.
- Add per-connection TCP receive buffering so HTTP headers and bodies can span
  multiple packets.
- Add `/webd` and `/fat/bin-webd`, the first persistent ring-3 web server for
  sequential HTTP requests.
- Add a dedicated background userspace process slot for `/webd`, exposed through
  monitor `bg`, `ps`, and `kill` commands.
- Add e1000 MMIO mapping revalidation on poll/TX paths so network I/O remains
  stable across cooperative kernel-thread scheduling.
- Replace the single background slot with a small process table, per-process
  page tables, scheduler-bound user context, and independent kernel trap stacks.
- Allow `run /path` to execute a foreground ELF while a background userspace
  process remains alive.
- Move scheduler stacks above 1 MiB so larger cooperative kernel worker stacks
  cannot overlap the legacy low-memory hole.
- Add timer-driven preemption from IRQ0/LAPIC timer ticks, including preemptive
  scheduling of CPU-bound ring-3 tasks.
- Add `/spin` and `/fat/bin-spin`, a busy-loop userspace app used to prove the
  monitor and `/webd` continue running without voluntary syscalls.
- Reap killed CPU-bound userspace processes from the timer preemption path.
- Add scheduler wait queues and use them for keyboard input plus network
  `accept`/`read`, including kill wakeups for sleeping network waiters.
- Route the e1000 PCI interrupt line through the IOAPIC as a level-triggered,
  active-low interrupt.
- Enable e1000 RX/link interrupts, acknowledge device interrupt causes in the
  IRQ path, and wake the network worker from interrupt context.
- Put the network worker to sleep on an RX wait queue instead of continuously
  polling while idle.
- Add e1000 interrupt cause counters, spurious IRQ accounting, network worker
  drain counters, and a low-rate timer watchdog wake for lost-interrupt
  recovery.
- Add IRQ-backed COM1 serial input and merge it into the monitor input path so
  the monitor works through a headless serial console.
- Add a fixed-size reusable workqueue for deferred bottom-half work.
- Move e1000 RX wakeups through the workqueue, with a direct fallback if the
  queue is full.
- Refresh high-half kernel mappings when entering process address spaces and
  lazily repair LAPIC/e1000 MMIO mappings if an interrupt arrives in an address
  space missing those pages.
- Add recursive exFAT directory traversal and generate `/fat/bin/...` app paths
  while keeping the older flat aliases.
- Validate syscall user buffers and strings against present user page mappings
  before copying data.
- Add a high-half kernel mapping propagation registry for process address
  spaces, while retaining a guarded LAPIC EOI ensure path for MMIO safety.
- Allow TCP `accept` to return established connections before payload arrives,
  so `read` owns blocking payload readiness.
- Replace fixed per-slot TCP receive arrays with dynamically allocated
  per-connection receive buffers that grow up to 32768 bytes.
- Add a block-device registry, a memory-backed block-device implementation, and
  mount the generated exFAT volume through that block layer.
- Add a monitor `block` command for block-device inspection.
- Add an initial read-only AHCI driver that discovers PCI AHCI controllers,
  maps ABAR MMIO, initializes SATA ports, issues IDENTIFY and READ DMA EXT, and
  registers disks such as `ahci0` with the block layer.
- Add `run-ahci-net` to boot with e1000 networking and an AHCI-attached copy of
  the generated exFAT image.
- Teach the exFAT reader to fetch boot sectors, FAT entries, directory clusters,
  and file data through the generic block-device API instead of requiring one
  contiguous memory image.
- Mount `/fat` from `ahci0` when an AHCI exFAT disk is present, with the
  memory-backed initramfs block device retained as the fallback.
- Add VFS read-buffer release hooks so exFAT heap-backed file reads can be
  freed after monitor probes, ELF loading, and process file close/exit paths.
- Add block-device write callbacks and AHCI `WRITE DMA EXT`, including
  read-modify-write for partial-sector updates.
- Add a first writable exFAT subset for AHCI-backed volumes: root-level file
  creation, overwrite of existing contiguous files, allocation bitmap updates,
  FAT entry marking, stream length updates, and VFS size refresh.
- Add monitor and userspace shell `write /fat/name text` commands backed by a
  simple filesystem-write syscall.
- Verify persistence by creating files on an AHCI-backed exFAT image, rebooting
  QEMU with the same disk, and reading those files back.
- Add a small generic block cache under `block_read`/`block_write`, with
  write-through updates, read-modify-write support for partial blocks, and
  monitor-visible hit/miss/write counters.
- Replace the exFAT global mounted-volume state with per-mount objects, each
  owning its own volume geometry, file records, paths, mountpoint, and write
  lookup state.
- Add monitor `mount` support for listing exFAT mounts and mounting a registered
  block device at a runtime mountpoint, plus a `run-ahci2-net` target for two
  AHCI exFAT disks.
- Make VFS nodes stable slots so deregistering one mount does not relocate
  unrelated open node pointers.
- Add exFAT `unmount`, VFS prefix deregistration, and a monitor `unmount`
  command that refuses busy mountpoints with open process file descriptors.
- Add static HTML/text files to the generated exFAT image and teach `/webd` to
  serve root-level `/fat` files over HTTP with basic path validation, MIME
  types, `GET`, `HEAD`, and 404/405/400 responses.
- Move AHCI to a non-conflicting high-half MMIO window, keep command submission
  in a short interrupt-disabled kernel-address-space section, and avoid carrying
  no-execute bits into intermediate VMM page-table entries.
- Add the first userspace support library under `userspace/lib` with syscall
  wrappers, `conio`-style cursor/clear/keyboard APIs, framebuffer `gfx`
  helpers, no-SSE userspace build flags, and `/fat/bin/ui` as a demo app.
- Teach both the monitor and userspace shell `ls [dir]` to show shallow
  directory contents by inferring child directories from VFS paths, and add a
  tiny userspace shell PATH resolver for `/fat/bin` plus `/`.
- Add a first PS/2 mouse driver: enable the auxiliary device, route IRQ12,
  parse 3-byte movement packets, expose nonblocking userspace delta/button
  polling through `srvros/mouse.h`, and update `/fat/bin/ui` with a mouse cursor.
- Add a first userspace window/widget toolkit in `srvros/ui.h`: buffered
  element surfaces, parent/child composition, top-down event dispatch with a
  returned consumer, hit testing, mouse/keyboard events, dirty marking, and
  root desktop presentation over the hardware framebuffer.
- Add `/fat/bin/desktop`, a first toolkit demo with a screen-backed fullscreen
  root, a buffered draggable window, buffered child panel/button widgets, mouse
  capture, keyboard exit handling, and cursor redraw by refreshing the affected
  screen rectangles.
- Split `/fat/bin/desktop` into a tiny userspace window server and freestanding
  `/fat/bin/calcgui` plus `/fat/bin/notesgui` client processes. The desktop
  launches those ELFs in the background, receives fixed-size GUI IPC messages
  describing windows/labels/buttons/text updates, renders the widgets, and sends
  click/close events back to the owning process. GUI queues are protected against
  preemptive scheduler interleaving.
- Add shell redirection, command sequencing, script sourcing, foreground and
  background job tracking, `wait`, `service webd`, and a login mode that runs
  `/fat/etc/init.sh`.
- Expand userspace CLI tools with `cp`, `rm`, `mkdir`, `mv`, `stat`, `grep`,
  `head`, `wc`, `ps`, and `kill`.
- Add exFAT append, unlink, nested directory creation, native file rename, empty
  directory rename, empty directory removal, and non-empty `rmdir` rejection.
- Add DHCP configuration from userspace, kernel network status reporting, ARP
  resolution for outbound UDP, and DNS A-record resolution through the DHCP DNS
  server or QEMU fallback DNS.
- Add `/fat/bin/textedit` and `/fat/bin/imgedit`, plus BMP helpers in the
  userspace library.
- Add smoke tests for CLI behavior, process lifecycle, nested directories,
  rename/rmdir, DHCP, DNS, web serving, GUI launch, and filesystem stress.
- Add the first POSIX-compat userspace layer with errno, malloc, file and
  directory wrappers, cwd handling, basic time, getpid, IPv4 helpers,
  DNS-backed `getaddrinfo`, TCP server socket wrappers, and `/fat/bin/posixdemo`
  as a smoke app.
- Add `ports/upstream` with pinned zlib and Lua submodules to start common
  library porting without vendoring snapshots into the main tree.
- Add minimal `stdio` support in the userspace library and `/fat/bin/zlibdemo`,
  which links pinned zlib, compresses data, writes the compressed stream to
  exFAT, reads it back, and verifies decompression.
- Increase the generated exFAT test volume and make image generation fail fast
  if future ports exceed the declared cluster heap.
- Expand the early POSIX/libc layer with `setjmp`/`longjmp`, `ctype`,
  C-locale helpers, signal stubs, additional string/stdio/stdlib/time calls,
  and integer-safe math macros.
- Add `/fat/bin/lua`, a Lua 5.4.8 interpreter built from a generated clean
  upstream copy in an integer-number srvros profile, with repeated script and
  `-e` smoke coverage.
- Make process-exit teardown non-preemptible while switching the scheduler
  thread back to kernel context and freeing the exiting address space.

## Next milestones

1. Harden writable exFAT: truncate growth, broader fragmented FAT-chain
   allocation tests, better rollback on partial rename/write failures, explicit
   `sync`, dirty-cache writeback, and crash-consistency documentation.
2. Add interrupt-driven AHCI command completion instead of purely polling
   commands.
3. Add NVMe discovery and read/write support as the second storage backend.
4. Add userspace socket APIs for nonblocking readiness, UDP sockets, response
   length/file metadata, and multi-worker web server designs; `/webd` now serves
   static files but still handles accepted connections sequentially.
5. Add a simple userspace filesystem server interface for experimental
   FUSE-like mounts.
6. Grow the userspace library into a real libc-shaped layer: shared startup,
   fuller `stdio`, scan/format helpers, better allocator growth, and common app
   build rules.
7. Add kernel-supported graphics buffer allocation/mapping so full-screen
   desktops and larger app windows are not constrained by static ELF BSS size.
8. Extend GUI IPC from server-rendered controls to client-owned surfaces:
   shared/mapped buffers, damage rectangles, focus tracking, keyboard delivery,
   and richer pointer events.
9. Expand syscall validation to include structured copyin/copyout helpers and
   better process termination on bad pointers.
10. Replace the LAPIC EOI mapping guard with a stronger invariant once all
   interrupt entry paths switch through a known kernel mapping context.
11. Add FPU/SSE save/restore for full floating-point userspace ports.
