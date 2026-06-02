# Release Notes

## Initial Repository Milestone

This milestone captures srvros as a bootable x86_64 research OS with a working
kernel, minimal userspace, filesystem mutation, networking, and a background web
server.

### Highlights

- Boots a higher-half x86_64 kernel through Limine.
- Runs freestanding ring-3 ELF programs from initramfs and `/fat`.
- Adds runtime ELF TLS support: the linker emits `PT_TLS`, the kernel maps the
  main-thread and per-user-thread TLS blocks, the scheduler preserves `FS.base`,
  and `/fat/bin/tlsprobe` verifies the path.
- Boots the experimental stripped Node.js runtime from a dedicated exFAT image:
  `make node-runtime-image` packages `/fat/bin/node`, and
  `tools/node_runtime_smoke.py` verifies `node --version` prints `v24.16.0` and
  exits cleanly under QEMU.
- Adds USB HID boot mouse support on the xHCI path and extends the hidden-QEMU
  UEFI USB smoke to attach both USB keyboard and mouse devices, type a monitor
  command, send a synthetic pointer move, and verify per-device input reports.
- Extends xHCI diagnostics with USB descriptor class/vendor/product summaries
  and hub-like device counts so real-machine input failures are easier to sort.
- Adds one-layer USB2 hub enumeration on xHCI, including hub port power/reset,
  route-string child addressing, parent/port diagnostics, and hidden-QEMU smoke
  coverage for keyboard and mouse devices behind a hub.
- Adds a generic absolute HID pointer path for QEMU `usb-tablet`, including
  QMP absolute-input smoke coverage directly on xHCI root ports and behind a hub.
- Adds monitor `hwdiag` and an A1466 first-boot checklist so real-machine runs
  can capture display, ACPI, PCI, xHCI, storage, memory, network, mounts, fsck,
  process, and boot-log state with one command.
- Adds `gfx_blit_rect` and teaches the UI presenter to flush root backbuffer
  dirty rectangles through a bulk graphics syscall instead of many tiny fills.
- Adds `/fat/bin/displayd`, a smoke-testable compositor seed with a dynamically
  allocated root backbuffer, resolution-aware layout metrics, GUI IPC server
  registration, hidden-QEMU smoke coverage, and dirty-rectangle cursor refresh.
- Adds kernel-managed GUI surfaces plus v2 surface-window/damage messages.
  `/fat/bin/surfacedemo` creates a drawable surface, blits pixels into it, and
  `displayd` composites it through the new app-owned-surface path.
- Routes GUI v2 configure, focus, pointer, and key events from `displayd` back
  to surface clients; `surfacedemo` now receives those events and redraws its
  client-owned surface in response.
- Adds `gui2`, an app-side helper library for v2 surface windows, dirty
  presents, event polling, and first button/textbox widgets. `/fat/bin/gui2demo`
  exercises that path under the displayd smoke.
- Gives `gui2` a shared theme, row layout helper, focus-aware widget dispatch,
  and textbox cursor support, then adds `/fat/bin/notes` as the first small
  GUI2 utility app. The displayd smoke now launches and maps it beside the raw
  surface and widget demos.
- Adds the first active `displayd` window-manager behavior for GUI2 surfaces:
  compositor-owned close/minimize buttons, title-bar dragging, z-order raise on
  focus, and app-side close-event handling in the GUI2 sample apps.
- Adds `tools/displayd_frame_smoke.py`, a hidden-QEMU/QMP regression smoke for
  the new v2 frame controls: focus, minimize/restore, drag, close, and clean
  compositor shutdown.
- Adds compositor dock launchers to `displayd` for GUI2 apps, including
  work-area-aware placement, duplicate-window staggering, unavailable-launcher
  handling, and `tools/displayd_launcher_smoke.py` coverage for launching
  Notes, GUI2 Demo, and Surface Demo from the dock.
- Adds bottom-right resize grips for `displayd` GUI2 windows. The `gui2`
  library can recreate kernel-managed backing surfaces on configure events, and
  the GUI2 demo, Notes, and Surface Demo redraw into their resized surfaces.
- Adds `tools/displayd_resolution_smoke.py`, which builds temporary
  resolution-specific Limine ISOs and verifies `displayd` at 800x600,
  1280x800, 1440x900, and 1920x1080.
- Adds `/fat/bin/calc`, a resizable GUI2 calculator client with an app-owned
  surface, integer arithmetic controls, configure/resize handling, and dock
  launcher smoke coverage, replacing the legacy `/fat/bin/calcgui` packaging.
- Moves `/fat/bin/textedit` onto GUI2 with an app-owned surface, resize-aware
  document layout, line entry, save/clear controls, and dock launcher smoke
  coverage.
- Moves `/fat/bin/paint` onto GUI2 as the shipped BMP paint/image editor, with
  an app-owned scaled canvas, palette controls, clear/save actions, and dock
  launcher smoke coverage.
- Adds a reusable GUI2 canvas widget and refactors paint to use it for scaled
  pixel drawing and pointer-to-pixel hit testing.
- Adds `/fat/bin/gui` as the stable GUI entrypoint. It execs
  `/fat/bin/displayd`, forwards compositor arguments, is shipped in both exFAT
  and initramfs packaging, appears in shell help, and is covered by dock
  launcher smoke coverage.
- Adds `tools/displayd_paint_smoke.py`, a hidden-QEMU/QMP paint regression that
  launches `/fat/bin/gui`, opens PAINT from the dock, clicks the canvas, clicks
  Save, and verifies the paint-side save marker without requiring a visible
  QEMU window.
- Hardens `displayd` client lifecycle handling with centralized client removal,
  periodic background-process reaping, close-timeout escalation, stale
  focus/hover/drag/resize cleanup, and frame smoke coverage for close cleanup,
  relaunch, and resize after relaunch through `/fat/bin/gui`.
- Adds a running-window taskbar to `displayd`. The bottom status band now shows
  GUI client entries; clicking an entry raises/focuses that window or restores
  it if minimized, with hidden-QEMU frame smoke coverage for taskbar restore
  and focus switching.
- Adds an on-screen Exit control to `displayd`. Session shutdown now requests
  close from all GUI clients, waits for cooperative destroy/exit cleanup, and
  escalates lingering clients before returning to the shell; the frame smoke
  verifies the full shutdown path.
- Consolidates the supported GUI launch path around `/fat/bin/gui`, documents
  `ui` and `desktop` as legacy regression tools, adds a `/fat/share/help/gui.txt`
  topic, and expands the A1466 first-boot checklist with first GUI launch,
  app/window interaction, taskbar restore, and clean Exit validation.
- Adds `tools/a1466_rehearsal.py` plus `make a1466-rehearsal`, a hidden-QEMU
  rehearsal for the MacBook Air A1466 USB boot path. It boots the generated USB
  image through OVMF/q35/AHCI/xHCI, verifies USB keyboard input, runs the
  first-boot diagnostics, starts the supported GUI smoke path, and writes
  `build/a1466-rehearsal.log`.
- Hardens the COM1 serial driver against missing UARTs. `serial_init` now
  probes COM1 with loopback before enabling it, and transmit waits are bounded
  so QEMU launches with `-serial none` cannot wedge the kernel during boot.
- Hardens PS/2 bring-up for real hardware without a usable 8042 controller.
  Keyboard init now detects `0xff` status, bounds the controller drain loop,
  returns availability to the main boot path, and skips PS/2 IRQ/mouse setup
  when unavailable so USB HID/xHCI input can continue booting.
- Advances the Node sqlite bridge: public `node:sqlite` now works on srvros
  through a transitional JavaScript shim, the Express/JWT demo uses and verifies
  that backend, and `node_sqlite.cc`, `node_webstorage.cc`, and bundled SQLite
  compile/link through the srvros probe. The native binding profile still stays
  off until the `HAVE_SQLITE=1` dispatch path no longer faults inside V8
  module-object setup.
- Expands the transitional `node:sqlite` shim with named bindings, simple
  filters, ordering, counts, filtered deletes, `UPDATE`, `LIMIT`, stricter
  binding errors, and a two-process persistence smoke against one exFAT image.
- Adds `tools/node_app_suite_smoke.py`, a hidden-QEMU Node app compatibility
  suite covering synchronous `fs`, timers, `path`, `url`, `querystring`,
  `events`, the srvros `crypto` HMAC shim, the srvros `fs/promises` shim, and
  the SQLite update/limit path. General projected-column SQLite aliases plus
  stream-heavy promise APIs remain documented follow-ups.
- Routes `fs/promises` through a transitional srvros shim backed by synchronous
  `fs`, giving Node packages Promise-shaped basic file I/O, directory listing,
  metadata, removal/rename/copy, and compact `FileHandle` support without
  entering the native FSReqPromise path that previously faulted.
- Adds a srvros file-stream shim for `fs.createReadStream()`,
  `fs.createWriteStream()`, and `FileHandle` read/write streams. The Node app
  suite now verifies file stream reads, writes, `Readable.from()`, and
  `pipeline()` copy behavior under hidden QEMU.
- Adds a srvros directory shim for `fs.opendir()`, `fs.opendirSync()`,
  `fs.promises.opendir()`, and `require('fs/promises').opendir()`, with callback
  reads, sync reads, async iteration, and stat-backed `Dirent` checks. The same
  path fills `readdirSync({ withFileTypes: true })` when the native srvros
  binding returns plain names.
- Adds srvros JS fallbacks for recursive `mkdir()`/`mkdirSync()` and
  polling-backed `fs.watchFile()`/`fs.unwatchFile()`, `fs.watch()`, and
  `fs.promises.watch()`. The Node app suite now bundles and verifies real
  `accepts`, `cookie`, `mime-types`, and `qs` package behavior under hidden
  QEMU.
- Adds `--rounds` to the Express/JWT/SQLite smoke harness and closes the Node
  milestone with a rebuilt runtime image plus hidden-QEMU passes for
  `node --version`, the app suite, SQLite persistence, 4 static HTTP route
  rounds, and 4 Express API route rounds.
- Extends the Node runtime harness to probe `--eval` and script text, and adds
  the first conservative Linux-libuv compatibility shims for pseudo-epoll,
  pseudo-eventfd, and best-effort `pipe2` flag handling.
- Adds `/fat/bin/cxxprobe` as a standalone C++ runtime sanity check and moves
  the Node JavaScript smoke boundary past the tracing-agent null-controller
  assertion by guarding against the temporary Linux-libuv/srvros-`uv_loop_t`
  layout mismatch.
- Moves the Node JavaScript smoke boundary through V8 platform worker startup,
  cppgc initialization, and no-access virtual reservations by fixing
  pseudo-epoll infinite waits, using conventional abort status 134, accepting
  advisory `MAP_NORESERVE`, and adding lazy kernel `PROT_NONE` mmap
  reservations.
- Adds `/fat/bin/cxxstlprobe` for libc++ `std::set`/`std::map`/`std::vector`
  coverage, fixes normal `malloc()` payloads to be 16-byte aligned, adds the
  libc++ verbose-abort runtime hook, and moves the current `node -e` boundary
  past V8 `RegionAllocator` construction to read-only snapshot deserialization.
- Adds framebuffer-console parsing for a compact ANSI CSI subset covering
  cursor movement, cursor positioning, clear screen, and clear line while
  preserving raw escape output on serial.
- Adds a minimal console TTY/termios layer with `tcgetattr`/`tcsetattr`,
  canonical/raw input mode toggles, `ICRNL`, `ECHO`, `VMIN`/`VTIME`, erase,
  kill-line, EOF control characters, `ioctl` window-size support, and duplicated
  stdio TTY detection, plus `/fat/bin/ttydemo` smoke coverage.
- Adds a first process-group/session control surface: kernel-tracked session
  ids, foreground TTY process-group queries/updates, libc
  `getpgrp`/`setpgid`/`getsid`/`setsid` and `tcgetpgrp`/`tcsetpgrp`, and
  `/fat/bin/posixdemo` coverage for the new wrappers.
- Makes futex waits signal-aware without tearing down their wait table entries,
  and keeps pthread mutexes, condition waits, and `pthread_once` POSIX-shaped by
  dispatching caught signals internally while timed condition waits preserve
  their real timeout deadline. `posixdemo` and `threadstress` cover the signal
  wake path.
- Adds libc `pthread_kill`, wakes `pthread_join` waits for caught signals while
  preserving non-`EINTR` pthread semantics, and extends `threadstress` with
  create/join and detached-thread lifecycle coverage.
- Adds libc `pipe2` and `dup3` wrappers with `O_CLOEXEC`/`O_NONBLOCK` flag
  handling and `/fat/bin/posixdemo` coverage.
- Adds `fcntl(F_DUPFD/F_DUPFD_CLOEXEC)` plus `mkostemp`, extending the
  temporary-file and fd-duplication surface expected by configure scripts and
  source ports.
- Applies POSIX-style `open(..., O_CREAT, mode)` mode handling,
  `open(O_CLOEXEC)` descriptor flags at creation, and
  `open(O_CREAT|O_EXCL)` existing-path rejection, giving source ports and
  `mkostemp` safer no-clobber behavior.
- Adds port-readiness libc surface for `limits.h`, `lstat`, `realpath`,
  `scandir`, and `alphasort`, with `/fat/bin/posixdemo` and
  `tools/ports_smoke.py` coverage.
- Adds static-first `execinfo.h` and `dlfcn.h` compatibility stubs plus
  `/fat/bin/nodeprobe`, a focused Node.js-readiness smoke app for time/random,
  mmap, fd, pthread, resource/accounting, socketpair/resolver, libuv, and
  diagnostic-loader probes.
- Expands Node/V8-oriented libc compatibility with `sys/param.h`,
  `sys/sysinfo.h`, `sys/auxv.h`, `sys/prctl.h`, `sys/syscall.h`, `madvise`,
  `pthread_getattr_np`, and single-CPU affinity stubs.
- Expands the Node srvros compile bridge from two entry objects to a manifest
  driven 10-object batch, adds filtered `libnode.a` replacement support in the
  link probe, and fills the exposed libc header gaps for `<inttypes.h>`,
  `<strings.h>`, C++ linkage guards, BSD string aliases, and integer conversion
  helpers.
- Adds a focused srvros libc++ probe archive for the Node bridge and broadens
  the libc/POSIX compatibility frontier with C23/fortified aliases, `*64` file
  wrappers, root uid/gid helpers, pthread naming/affinity shims, terminal raw
  mode, extra math/time/wide-char helpers, service/name lookup, and conservative
  Linux event/discovery stubs, shifting the Node link frontier toward C++
  standard-library coverage and srvros-compiled object consistency.
- Adds `/fat/bin/ed`, a compact scriptable line editor for early source-port
  workflows, with edit/write/compare coverage in `tools/ports_smoke.py` and a
  port manifest at `ports/srvros/ed/PORT.md`.
- Adds first-pass libc `regex.h` support for `regcomp`, `regexec`, `regerror`,
  and `regfree`, then moves `grep` and the existing `sed` subset onto the
  shared matcher for regex addresses/substitutions and fixed-string fallback.
- Extends the compact regex engine with alternation, capture offsets, and
  bounded repeats; adds `grep -o/-l/-L`, sed numbered replacement expansion,
  `ed` regex address/substitution support, and `tools/regex_smoke.py` as a
  focused text-stack harness.
- Expands the generated exFAT image to 128 MiB with a matching FAT reservation
  so the growing userspace and smoke-test write workload still have headroom.
- Schedules kernel threads and userspace processes with timer preemption.
- Provides foreground/background process control through the monitor and shell.
- Expands shell job tracking to 32 entries and makes bare `wait` drain every
  tracked shell job instead of only the current background job.
- Raises process and VMM address-space capacity to 64 slots plus scheduler
  thread capacity to 32 slots, recycles dead scheduler-thread stacks before slot
  reuse, improves pipeline spawn failure diagnostics, and adds
  `tools/process_pressure.py` for repeated exec, pipeline, and background-job
  pressure coverage.
- Mounts exFAT from initramfs-backed memory or AHCI-backed disks.
- Supports exFAT file create/write/append/delete/rename, directory create,
  empty directory removal, mount/unmount, and consistency checks.
- Drives an Intel e1000 NIC in QEMU with interrupt-backed receive handling.
- Supports ARP, ICMP echo, DHCP, DNS A-record resolution, enough TCP for a
  userspace HTTP server, and client-side TCP connect for simple outbound HTTP.
- Keeps accepted TCP streams alive when their listener fd is closed, matching
  the server lifecycle expected by POSIX/libuv-style event loops.
- Pins upstream libuv `v1.52.1` as `ports/upstream/libuv` and adds
  `/fat/bin/libuvdemo` plus `tools/libuv_smoke.py` as the dedicated staging
  harness for replacing the srvros `uv.h` adapter with upstream-backed pieces.
  `uv_version()` and `uv_version_string()` now link upstream `src/version.c`,
  and the adapter exposes libuv-style errno constants/name/string helpers.
  libc also defines C-standard `EDOM` so upstream libuv can correctly detect
  positive system errno values.
- Expands the libuv adapter's core API surface with loop close/data helpers,
  backend timeout/fd reporting, handle/request type/name/size/data helpers,
  active/closing checks, and `uv_timer_get_due_in` smoke coverage.
- Expands libuv-shaped TCP stream behavior with writable-readiness connect
  completion, deferred queued writes, pending write-byte accounting, an
  outbound guest-to-host TCP smoke path, and close-from-callback guards for
  poll snapshots.
- Moves libuv listener/accept/read/write APIs toward generic `uv_stream_t`
  signatures and adds queued `uv_shutdown` support for TCP streams.
- Adds more libuv loop parity with prepare/check/idle phase handles, poll
  disconnect/error mapping, timer repeat helpers, and queued `uv_getaddrinfo`
  callbacks over the srvros resolver.
- Expands libuv filesystem parity with lstat/fstat/access/realpath/scandir,
  request getters, cleanup-owned allocations, and queued fs callbacks through
  `uv_run`.
- Adds libuv handle lifetime helpers for `uv_ref`, `uv_unref`, `uv_has_ref`,
  `uv_walk`, and `uv_fileno`, so unreferenced handles no longer keep the loop
  alive by themselves.
- Adds first-pass libuv process/stdio staging with `uv_pipe`, `uv_pipe_t`,
  `uv_spawn`, `uv_process_t`, child stdout pipe wiring, and
  `waitpid(WNOHANG)` exit callbacks.
- Expands libuv process/stdio staging with child stdin pipes, cwd-scoped
  `uv_spawn`, and duplex child stdio pipes backed by a kernel pipe-pair fd
  primitive.
- Hardens libuv `uv_spawn` validation so empty executables, empty argv,
  unsupported process flags, invalid stdio source combinations, bad inherited
  fds, missing executables, and bad cwd fail before registering a process
  handle or leaving stdio resources behind.
- Raises per-process open-fd capacity for port-heavy workloads, grows the
  backing read/write/pipe pools, reports `_SC_OPEN_MAX`, aligns `FD_SETSIZE`,
  and adds `posixdemo`/`ports_smoke.py` coverage for many open files and pipes.
- Restores libuv inherited-fd stdin spawn coverage and caps process-only
  `uv_run` waits so child exits are observed without another fd event.
- Adds libuv inherited-stream spawn coverage for stdin/stdout/stderr and a
  short-lived process-only spawn loop to guard exit polling behavior.
- Raises POSIX/kernel poll snapshots to 64 entries, expands libuv's poll
  snapshot, and adds high-fd `select` plus multi-handle poll readiness coverage.
- Adds `netinet/tcp.h`, `TCP_NODELAY` socket-option storage/no-op handling,
  libuv `uv_tcp_nodelay`/keepalive/open helpers, and `netcheck` coverage for
  nonblocking TCP connect readiness plus `getsockopt(SO_ERROR)`.
- Adds TCP `MSG_PEEK` through a kernel peek syscall, exposes `accept4`, and
  tracks socket options on accepted real TCP fds so libuv/POSIX streams can
  set/query `TCP_NODELAY`, keepalive, linger, and buffer options after accept.
- Adds `socketpair(AF_UNIX, SOCK_STREAM)` over the existing duplex pipe-pair
  primitive, FIFO `fstat` metadata for pipe-like fds, and libuv
  `uv_socketpair` smoke coverage.
- Adds `sys/uio.h` with `readv`/`writev`/`preadv`/`pwritev` plus
  `sendmsg`/`recvmsg` scatter/gather socket wrappers and bounded
  `SCM_RIGHTS` fd passing over local socketpairs.
- Adds pathname `AF_UNIX` stream sockets backed by kernel bind/listen/connect/
  accept queues, libc `sys/un.h`, `unlink` unbind support, and
  `posixdemo`/`libuvdemo` coverage for local socket transfer.
- Adds libuv `uv_pipe_bind` and `uv_pipe_connect` staging over srvros pathname
  local sockets.
- Adds first-pass libuv IPC handle passing with `uv_write2`,
  `uv_pipe_pending_count`, `uv_pipe_pending_type`, and `uv_accept` of a pending
  pipe handle over an IPC pipe.
- Extends libuv IPC staging with a bounded pending-handle queue and smoke
  coverage for multiple `uv_write2` transfers before accept.
- Extends the libuv IPC smoke path across `uv_spawn` by creating a duplex IPC
  stdio pipe for a child, sending a live pipe handle with `uv_write2`, and
  having the child accept and write through that transferred handle. Credentials
  and broader ancillary messages remain future work.
- Adds socket-mode metadata for net fds, net fd cloning through the kernel
  rights path, TCP handle classification in `uv_guess_handle`, and
  `/fat/bin/libuvdemo` coverage for `uv_write2` transfer of a TCP listener
  handle with `UV_TCP` pending-handle typing.
- Refcounts TCP listener/connection and UDP kernel handles across fd
  duplication, inheritance, and `SCM_RIGHTS` transfer, and extends the
  libuv TCP IPC smoke path so a received TCP handle remains valid after the
  sender closes its original fd.
- Adds `/fat/bin/tcpstress` and `tools/tcpstress_smoke.py` for host-forwarded
  POSIX TCP close-order pressure, covering listener and accepted-connection
  `dup`/close survival, socket `fstat`, `accept4`, name queries, reply writes,
  shutdown, and EOF handling. libc now maps pseudo socket `fstat` and `dup`
  onto the underlying kernel fd.
- Extends `/fat/bin/tcpstress` with a `ready` mode and adds
  `tools/tcp_ready_smoke.py` for host-driven TCP readiness coverage: empty and
  repeated listener polls, accepted-stream `POLLIN`/`POLLOUT` timing, peer
  payload readiness, and post-reply cleanup.
- Adds libuv thread/synchronization wrappers over srvros pthreads, covering
  thread create/create-ex/detach/join/self/equality, mutexes, recursive mutex
  initialization, condition variables, reader/writer locks, semaphores,
  barriers, `uv_once`, and TLS keys in `/fat/bin/libuvdemo`.
- Moves libuv queued work and callback-based filesystem requests onto a small
  reusable pthread worker pool, adds per-loop wake pipes, and extends
  `/fat/bin/libuvdemo` to cover multi-request work and async fs completions.
- Adds `uv_cancel` support for work and async filesystem requests that are
  still queued in the libuv worker pool, returning `UV_EBUSY` once a worker has
  started the request.
- Tightens libuv loop-close behavior so inactive but unclosed handles still keep
  `uv_loop_close` busy until `uv_close` has been called.
- Adds libuv TTY and signal staging with `uv_guess_handle`, `uv_tty_t`
  window-size/mode/write helpers, vterm-state probes, and `uv_signal_t`
  start/stop/one-shot callback delivery for SIGINT/SIGTERM.
- Adds libuv platform and filesystem parity for cwd/chdir, kernel-reported
  exepath, env/environ, process title, home/tmp paths, single-user passwd/group,
  uname, uptime, load average, resource usage, CPU/interface enumeration,
  pid/ppid, hrtime, memory totals, sync/queued random fills, fsync/fdatasync,
  ftruncate, sendfile, utime, futime, and `uv_fs_poll` file-change polling,
  backed by kernel process metadata, meminfo/random/time mutation syscalls, and
  expanded `/fat/bin/libuvdemo` coverage.
- Ships `/fat/bin/webd`, a poll-driven ring-3 web server serving static files
  from `/fat/www` with nested asset paths, content lengths, MIME/cache headers,
  idle cleanup, segmented larger TCP responses, a bounded active-client table,
  lightweight `webd: stats` observability, and a plain-text `/__status`
  endpoint. Runtime options now cover `--root`, `--max-clients`,
  `--stats-every`, and `--quiet`, while request parsing rejects unsupported
  HTTP versions and oversized request lines/headers with clearer status codes.
- Adds `tools/webd_soak.py` to exercise `webd` under repeated host HTTP
  requests, a small concurrent client burst, early client disconnects during a
  larger response, malformed requests, a final post-abort request, and
  `/__status` counter validation.
- Adds config-backed shell services under `/fat/etc/services/*.svc`; the
  generated image ships `webd.svc`, starts `/init --system` from the kernel,
  logs startup output to `/fat/var/log/init.log`, launches `/fat/bin/svscan`
  from `/fat/etc/init.sh`, restarts services marked `restart=always`, exposes
  `service list`, `service enable`, `service disable`, `service reload`,
  `service log`, `service tail`, and `service supervise`, and keeps daemon
  stdout readable in `/fat/var/log/webd.log` with supervisor events in
  `/fat/var/log/svscan.log`.
- Tightens `svscan` service supervision by reaping all exited matching service
  processes before restart decisions, stopping disabled services, logging
  startup/missing/exited restart reasons, and logging crash-loop backoff.
- Extends service configs with `requires=network`, `health=listen:<port>`, and
  `max_log=<bytes>`; `svscan` now waits for dependencies, restarts unhealthy
  listeners, and rotates oversized daemon logs. The shell adds service-wide
  `status --all`/`restart <name>` helpers plus per-service `check`,
  `check-config`, and `rotate-log`.
- Adds `tools/service_soak.py`, a service-operations soak that keeps `webd`
  under repeated host HTTP traffic while checking service health, config
  validation, restart, log rotation, socket visibility, and svscan event logs.
- Adds shell service config editing with `service set`/`service unset`, bad
  config validation coverage, and `service restart <name> --wait` for
  health-gated restarts.
- Adds `/fat/share/help` topic files, shell `help <topic>` lookup, and a small
  `/fat/bin/more` pager with script-friendly `--plain` mode.
- Adds CLI discovery polish: `help -l`, `man <topic>`, `apropos <word>`,
  tab completion for help topics and service names/actions, generated
  `/fat/share/examples`, login `/fat/etc/profile.d/*.sh` snippets, and default
  `/fat/tmp` plus `/fat/home` directories in the generated exFAT image.
- Normalizes `-h`/`--help` usage output across the core CLI, service, and
  network utility set.
- Normalizes `--` option termination across the common file/text utilities used
  by the shell smoke path.
- Adds a shared POSIX utility applet installed as `ln`, `sync`, external
  `test`/`[`, `cksum`, `sum`, `comm`, `paste`, `join`, `split`, `od`,
  `hexdump`, `strings`, `file`, `tty`, `stty`, `time`, `timeout`, `nohup`,
  and `nice`, with `tools/posixutils_smoke.py` covering the aliases in QEMU.
- Wires `sync` through libc and the kernel so it flushes dirty process-owned
  writable file descriptors; the block cache remains write-through.
- Adds shell `$((expr))` integer arithmetic expansion with variables,
  parentheses, arithmetic operators, comparisons, and simple boolean operators.
- Raises the `srv_exec` environment vector capacity to 64 entries so login
  shells, `read`, and command-local `NAME=value` runs do not exhaust exec
  setup during longer CLI sessions.
- Raises the writable-file staging cap to 16 MiB and expands the exFAT/VFS
  directory/node tables, which lets recursive `cp` create deeper destination
  trees and lets CLI copies preserve larger binaries such as `/fat/bin/sh`.
- Adds fragmented exFAT allocation fallback for writable files, preserves stream
  flags across overwrite/rename, validates chained EOF and leaked allocated
  clusters in `fsck`, checks stale FAT entries on bitmap-free clusters, and
  expands `fs_stress.py` with fill/delete/copy/compare coverage.
- Adds `tools/fsck_corrupt.py`, a QEMU-backed corruption harness that flips
  temporary exFAT images and verifies `fsck /fat` reports the expected failure.
- Adds directory-entry snapshot/restore helpers for exFAT create and rename
  paths so failed registration or entry writes roll back the affected entry run.
  `fs_stress.py` now covers short-to-long and long-to-short rename updates.
- Reorders exFAT file and empty-directory delete so VFS unregister failures
  restore the old directory entry before clusters are released.
- Ships `/fat/bin/dd` with block copy, `/dev/zero`, size suffixes, and
  generated large-file smoke coverage followed by `fsck /fat`.
- Deduplicates identical applet payloads in the generated exFAT image so many
  command aliases do not consume separate cluster chains on the default disk.
- Ships `/fat/bin/httpget`, a tiny outbound HTTP/1.0 client backed by
  DNS-backed `getaddrinfo`, POSIX `connect`, `send`, and `recv`.
- Adds userspace IPv4 UDP sockets with `sendto`/`recvfrom`, poll readiness,
  bounded receive queues, and `/fat/bin/udpdns` DNS-over-UDP smoke coverage.
- Ships `/fat/bin/netstat`, backed by a kernel socket-table enumeration syscall
  for TCP listeners/connections and UDP sockets with PIDs, endpoints, queues,
  send-window state, socket errors, and TCP lifecycle flags.
- Ships `/fat/bin/ifconfig`, `/fat/bin/route`, and `/fat/bin/arp`, backed by a
  structured kernel network status syscall for interface identity, DHCP
  route/DNS configuration, protocol counters, socket counts, worker counters,
  and current ARP resolution.
- Adds a small fixed ARP cache, cache enumeration for `/fat/bin/arp`, and
  `/fat/bin/ping` using kernel-backed ICMP echo requests.
- Tightens DNS resolution: kernel DNS retries queries, userspace DNS paths
  prefer DHCP DNS, fall back through `DNS_SERVER` or `/fat/etc/resolv.conf`
  where applicable, and ship `/fat/bin/host` for direct A-record lookup.
- Extends the socket compatibility layer with userspace UDP-backed
  `getaddrinfo`, `getsockname`, `getpeername`, `shutdown` validation,
  kernel-backed TCP shutdown, connected UDP shutdown state,
  `setsockopt(SO_REUSEADDR/SO_KEEPALIVE/SO_LINGER/SO_RCVBUF/SO_SNDBUF)`,
  `getsockopt(SO_ERROR/SO_TYPE/SO_ACCEPTCONN/SO_REUSEADDR/SO_KEEPALIVE/SO_LINGER/SO_RCVBUF/SO_SNDBUF)`,
  and
  `/fat/bin/udpecho` local datagram smoke coverage.
- Adds compact TCP close lifecycle states so connection close/shutdown paths can
  exchange FIN/ACK traffic before timer cleanup instead of immediately dropping
  all connection state.
- Adds ACK-tracked TCP transmit history with timer-based retransmission for
  SYN, FIN, and payload frames.
- Adds bounded TCP send backpressure so `POLLOUT` and nonblocking writes are
  driven by available transmit-history/window space instead of always reporting
  writable.
- Tracks peer-advertised TCP receive windows and uses them to cap payload sends
  alongside the local outstanding-send limit.
- Advertises dynamic TCP receive windows from unread buffer space, sends
  window-update ACKs after userspace reads, and uses a small zero-window persist
  timer plus idle cleanup for abandoned clients.
- Validates incoming IPv4/TCP/UDP checksums and returns TCP RSTs for closed
  ports, missing connection tuples, and full connection-table SYN attempts.
- Adds a tiny kernel socket-error query path so libc can report
  `SO_ERROR`, `ECONNREFUSED`, `ETIMEDOUT`, `ECONNRESET`, and in-progress
  nonblocking connects more accurately.
- Tightens TCP receive sequencing so duplicate or out-of-order data/FIN packets
  are acknowledged without corrupting the userspace byte stream.
- Includes a small shell, CLI utilities, service control, redirection,
  multi-stage pipelines, scripts, PATH lookup, and background jobs.
- Adds the first POSIX-compat userspace layer for file, directory, errno,
  malloc, `sbrk`, pipes, time, cwd, IPv4, DNS, and TCP socket APIs.
- Adds minimal `stdio`, stages zlib and Lua as pinned submodules under
  `ports/upstream`, ships `/fat/bin/zlibdemo`, and adds `/fat/bin/lua` as an
  initial Lua 5.4.8 interpreter.
- Adds pinned cJSON `v1.7.19` and inih `r62` submodules, plus
  `/fat/bin/jsondemo` and `/fat/bin/inidemo` smoke apps for JSON and INI
  parse/roundtrip coverage.
- Adds pinned linenoise `2.0` plus a srvros console adapter used by `srvsh` for
  editable prompt input, TAB completion, longest-common-prefix completion fill,
  and `/fat/.srvsh_history`; `/fat/bin/linedemo` verifies history save/load
  behavior.
- Tightens the linenoise adapter to use raw TTY mode while editing, with
  Ctrl-A/Ctrl-E movement, Ctrl-U/Ctrl-W kill, Ctrl-Y yank, escape-sequence
  arrows, and draft-preserving history browsing covered by
  `tools/shell_edit_smoke.py`.
- Adds a `history` shell builtin with `HISTFILE`/`HISTSIZE`, explicit
  read/write/clear controls, and script-path/line diagnostics for common shell
  errors.
- Adds SQLite `3.53.1` as a pinned amalgamation snapshot and
  `/fat/bin/sqlitedemo`, which registers a small srvros VFS and verifies
  create/insert/query/reopen behavior against `/fat/sqlitedemo.db`.
- Adds early newlib-style syscall hooks and kernel support for `fstat`,
  `O_RDWR` regular-file fds, relative/end-relative `lseek`, and process heap
  growth.
- Moves userspace `malloc` to `sbrk`-grown heap chunks and adds `dup`/`dup2`
  support for standard streams, pipes, writable regular files, and read-only
  regular files.
- Shares regular-file open descriptions across `dup`/`dup2` and child fd
  inheritance, so read-only descriptors share offsets and writable descriptors
  flush on last close.
- Adds `poll`/`select` support for standard streams, regular files, pipes, and
  TCP listener/connection fds, with pipe readiness and hangup smoke coverage.
- Adds `fcntl(F_GETFL/F_SETFL)` and `O_NONBLOCK` support for the first fd set,
  including pipe, listener, and connection `EAGAIN` behavior.
- Adds `fcntl(F_GETFD/F_SETFD)` and `FD_CLOEXEC` descriptor flags, including
  close-on-exec cleanup during process replacement and socket pseudo-fd flag
  propagation.
- Adds POSIX-style advisory byte-range locks through
  `fcntl(F_GETLK/F_SETLK/F_SETLKW)` for regular files, with `posixdemo`
  plus `/fat/bin/lockprobe` conflict coverage and SQLite VFS locking backed by
  the new kernel lock table.
- Adds real empty-file support, fd flush/truncate hooks, and POSIX-facing
  `access`, `isatty`, `fsync`, `truncate`/`ftruncate`, `chmod`/`fchmod`, and
  `umask` compatibility.
- Persists srvros Unix-like metadata for writable exFAT mounts in
  `/fat/.srvros/meta`, including reboot-tested inode/mode restoration, and
  ships `/fat/bin/chmod` for shell-level permission changes. Sidecar writes
  stage through `/fat/.srvros/meta.tmp`, and mount recovery promotes a complete
  temp file or removes a malformed one before applying metadata.
- Expands libc/POSIX coverage with `pread`/`pwrite`, `getopt`, `uname`,
  `posix_memalign`/`aligned_alloc`, `qsort`, `bsearch`, random numbers,
  integer and floating conversion helpers, process-local environment variables,
  more `stdio` positioning helpers, `atexit`, and newlib `_pread`/`_pwrite`
  hooks.
- Enables FPU/SSE/SSE2 for kernel and ring-3 code, preserves per-process and
  per-scheduler-thread SIMD state across traps, syscalls, scheduler switches,
  and kernel/user transitions with `fxsave64`/`fxrstor64`, and ships
  `/fat/bin/fpdemo` as a preemption stress test for userspace double math.
- Adds an initial userspace `math.h`/`float.h`, floating `%f`/`%g` formatting,
  and switches `/fat/bin/lua` to its normal floating profile with the stock
  `math` library enabled.
- Links all userspace programs through a shared `crt0.S` startup object, removing
  per-app copied `_start` assembly while keeping static ELF apps self-contained.
- Adds basic `scanf`/`sscanf`/`fscanf` support for integer, string, character,
  and floating conversions.
- Extends `scanf`/`sscanf`/`fscanf` with scansets, inverted scansets, simple
  ranges, assignment suppression coverage, and short/char integer destinations.
- Extends userspace formatted output with common width, precision, padding,
  sign, alternate-form, length, and `%n` handling, and backs `system()` with
  `sh -c` through `posix_spawnp`/`waitpid`.
- Adds simple full/line/unbuffered `stdio` buffering, stream EOF/error state
  cleanup, path-backed `fflush`, `setvbuf`/`setbuf`/`setlinebuf`, and
  `posixdemo` coverage for buffered read/write/seek behavior.
- Adds shell-backed `popen`/`pclose` for one-way process streams and teaches
  spawned children to inherit a parent process's redirected standard streams.
- Extends `posixdemo` stdio coverage for `w+`, `r+`, and `a+` update streams,
  including buffered read-to-write offset reconciliation and append-after-seek
  behavior.
- Adds first-pass anonymous private `mmap`/`munmap` support with process-owned
  page cleanup, libc `sys/mman.h` wrappers, and `posixdemo` coverage for
  zero-filled read/write mappings plus conservative `MAP_FIXED` mapping.
- Extends `mmap` to eager file-backed `MAP_PRIVATE` mappings from regular fds,
  including page-aligned offsets and `posixdemo` coverage that memory writes do
  not modify the source file.
- Adds `mprotect` and `PROT_NONE` support for mmap-owned pages, with page-table
  permission updates and `posixdemo` coverage for guard-page enablement plus
  read/write protection toggles.
- Adds `msync` constants, libc wrapper, syscall, and `posixdemo` coverage as
  no-op validation for private mmap-owned ranges.
- Adds shell `env`/`export`/`which` builtins and small `/fat/bin` compatibility
  tools for `which`, `env`, `pwd`, `true`, and `false`.
- Extends `/fat/bin/env` so it can run commands with temporary environment
  changes, `-i`, and `-u`, and teaches the shell builtin to hand command-mode
  `env` invocations to that tool.
- Adds first CLI milestone quality-of-life tools: `sleep`, monotonic-uptime
  `date`, `touch`, `basename`, and `dirname`.
- Adds more table-stakes CLI tools: `tail`, `tee`, `uname`, `hostname`, and
  `uptime`.
- Adds shell `for`/`in`/`do`/`done` loops, login profile loading from
  `/fat/etc/profile`, `PS1` prompt customization, and first filesystem traversal
  tools with `/fat/bin/find` and `/fat/bin/du`.
- Adds exFAT-backed filesystem capacity reporting through a small `statfs`
  syscall, POSIX-facing `statvfs`, and `/fat/bin/df`, plus pipe-friendly
  `/fat/bin/sort`, `/fat/bin/uniq`, and `/fat/bin/cut`.
- Adds lightweight shell functions with positional arguments, `return`, one-line
  and multiline script definitions, and `type` reporting for functions.
- Adds `/fat/bin/xargs`, a small literal-substitution `/fat/bin/sed`, and shell
  unmatched-quote diagnostics for friendlier CLI scripting failures.
- Adds `/fat/bin/mktemp`, `mkdir -p`, recursive `cp -r`/`rm -r`, shell
  unterminated-block diagnostics for scripts, and Ctrl-C prompt recovery in the
  linenoise console adapter.
- Adds `/fat/etc/profile` and shell fallback `TMPDIR=/fat/tmp`, makes default
  `mktemp` honor `TMPDIR`, decodes quoted assignment values after command
  substitution, and teaches `/fat/bin/mv` to move files and empty directories
  into existing directory destinations.
- Adds interactive Ctrl-D/EOF shell exit coverage and expands common text-tool
  flags: `grep -i/-n/-v/-c/-q`, `wc -l/-w/-c`, compact `head -1`/`tail -1`
  forms, and `find -type f|d`.
- Expands the next CLI compatibility slice with `ls -a/-l` combined flags and
  multi-path headers, `sed -n`, `sed -e`, literal `p`/`d` commands, simple
  line-number and `/pattern/` addresses, and `test -s/-r/-w/-x`.
- Extends the file-tool compatibility pass with `ls -d`/`ls -1`, clustered
  `rm -fRr`, no-error forced missing-file removal, and multi-source `cp`/`mv`
  into existing directory destinations.
- Adds `/fat/bin/expr` for script-friendly integer arithmetic, comparisons,
  string length/substr/index operations, and literal-prefix `:` matching.
- Adds `/fat/bin/printf` and `/fat/bin/tr` for portable script output and
  simple byte translation/deletion pipelines.
- Adds `/fat/bin/seq` and `/fat/bin/realpath` as small script-porting helpers
  for integer ranges and canonical checked paths.
- Adds `/fat/bin/id`, `/fat/bin/whoami`, `/fat/bin/readlink`, `/fat/bin/cmp`,
  and `/fat/bin/yes` to satisfy more common configure/build-script probes.
- Adds `/fat/bin/install`, `/fat/bin/diff`, and a small uncompressed ustar
  `/fat/bin/tar` for build install steps, file comparisons, and archive
  create/list/extract round trips.
- Adds `/fat/bin/gzip` and `/fat/bin/gunzip` on top of the pinned zlib port,
  including file and tarball round-trip smoke coverage.
- Adds `/fat/bin/minizip` and `/fat/bin/miniunz` from zlib's MiniZip contrib
  sources, with zip create/list/extract smoke coverage.
- Makes no-argument `/fat/bin/lua` a real interactive REPL with linenoise
  history on TTYs and stdin-fed chunk execution for pipelines.
- Adds the first pthread compatibility slice for porting probes: mutexes,
  condition variables, `pthread_once`, pthread TLS keys, basic attributes,
  `sched_yield`, `nanosleep`, and `getpagesize`/`sysconf`.
- Adds the first same-address-space pthread thread path:
  `pthread_create`, `pthread_join`, `pthread_detach`, and `pthread_exit`
  backed by native thread syscalls, per-thread user stacks, per-thread TLS
  values, per-thread user FPU state, and a dedicated scheduler kernel trap
  stack for spawned user threads.
- Hardens pthread teardown with `SYS_THREAD_STATUS`, libc-side reclamation of
  completed detached pthread stacks, and process-exit cleanup that retires
  sibling user-thread scheduler contexts before address-space teardown.
- Adds `SYS_FUTEX_WAIT`/`SYS_FUTEX_WAKE` as a compact process-local futex-style
  primitive and moves pthread mutex/condition-variable waits onto it, including
  timed condition waits in the POSIX smoke path.
- Hardens the pthread/libc sharing path by adding a futex-backed process-local
  heap lock, making `pthread_once` wait for in-progress initializers, and
  extending `posixdemo` with a small multi-threaded malloc/realloc/calloc
  stress check.
- Adds `/fat/bin/threadstress` and `tools/thread_smoke.py` to cover explicit
  user-thread yields, pthread mutex/condition/once behavior, TLS keys, pthread
  attributes, timed condition waits, recursive/error-checking mutexes, detached
  reaping, shared heap allocation, shared regular-file fd writes, and shared
  stdio stream writes under QEMU preemption.
- Fixes same-process user-thread context stability by updating the TSS from
  each thread's effective kernel trap stack and preserving the exact SysV
  userspace stack alignment selected by libc before entering a spawned pthread.
- Expands the pthread surface with stack attribute helpers, mutex attributes,
  condition attributes, recursive/error-checking mutex modes, and recursive
  futex-backed stdio stream locks exposed through `flockfile`,
  `ftrylockfile`, and `funlockfile`.
- Serializes regular-file `read`/`write`/`seek` offset updates in the kernel so
  shared descriptors are safer under preemptive same-process pthreads.
- Adds `/fat/bin/patch` for simple unified-diff application with `-i` and
  `-pN` support.
- Adds `/fat/bin/make` for small source-port recipes with variables,
  dependencies, `.PHONY`, automatic variables, and shell-backed commands.
- Adds `/fat/bin/byacc` from the pinned Berkeley Yacc snapshot as the first
  native source-port build generator, with smoke coverage for producing
  `y.tab.c` and `y.tab.h` from a small grammar.
- Expands configure-script compatibility with shell `test`/`[` boolean
  expressions, `!`, `-nt`, `-ot`, `-ef`, mandatory `]` checking, and
  `/fat/bin/xargs -n`/`-r`.
- Adds common long-option aliases used by configure/build scripts:
  `grep --regexp/--ignore-case/--quiet/--fixed-strings`, `sed --quiet` and
  `--expression`, `xargs --max-args`, `tar --create/--list/--extract --file`,
  `make --file/--dry-run/--always-make`, `find -print`, and long forms for
  `ls`, `cp`, `rm`, `mv`, `mkdir`, `install`, `tee`, `head`, `tail`, `wc`, and
  `ln`. `tools/configure_smoke.py` now covers these configure-style probes in a
  shorter QEMU path.
- Tightens shell `set -e` handling for same-line scripts: plain failing
  semicolon chains now stop before later commands, while configure probes using
  `&&`, `||`, `if`, and `while` conditions keep their expected exemption
  behavior. `tools/configure_smoke.py` now checks both required output and
  forbidden output markers for this path.
- Adds shell `while ... do ... done`, `shift`, and the no-op `:` builtin for
  more realistic script control flow.
- Adds shell `break`/`continue` loop control and the POSIX-style `command`
  builtin for `-v`/`-V` lookup and alias/function bypass execution.
- Adds first-pass shell `case`/`in`/`esac` handling with glob-style patterns,
  `|` alternatives, default arms, and multiline script support.
- Allows completed `if`/`for`/`while`/`case` compound commands to continue with
  `;`, `&&`, and `||` tails.
- Adds another shell compatibility slice: command-local environment assignments,
  current-shell brace grouping, comment handling, script line continuations,
  simple here-docs, and better quoted/escaped argument preservation.
- Adds shell `$!` expansion, `fg`/`bg` builtins for the current background
  process model, `wait` status propagation, and tab completion for builtins,
  aliases, functions, PATH commands, and filesystem paths.
- Adds shared userspace path normalization for shell/libc/syscall wrappers,
  including inherited `PWD`, relative paths, repeated `/`, `.`, and `..`.
- Adds kernel-routed Ctrl-C/SIGINT delivery to the active foreground process,
  SIGTERM-backed `kill`, and shell-visible `128 + signal` termination statuses.
- Extends foreground signal handling to shell pipelines by assigning native
  process groups at exec time, waking blocked pipe readers/writers on signal,
  and preserving interrupted pipeline status as `128 + signal`.
- Adds shell job-table tracking for background pipelines, allowing `cmd | cmd &`
  to be foregrounded with `fg $!` and interrupted as a process group.
- Adds shell job references (`%+`, `%-`, numeric `%N`), `jobs -l` pid listing,
  and built-in `kill %job` delivery across every process tracked for a job.
- Expands `kill` to POSIX-style signal targets: positive pids, caller process
  group (`0`), negative process-group ids, and signal `0` existence probes.
  The shell `kill` builtin and `/fat/bin/kill` now accept `-signal`, and libc
  exposes `kill`, `raise`, and first-pass `signal` handling for
  `SIGINT`/`SIGTERM`.
- Expands libc signal compatibility with `sigaction`,
  `sigprocmask`/`pthread_sigmask`, `sigpending`, and `sigwait` over the
  kernel catch/poll pending-signal path, with `/fat/bin/posixdemo` coverage.
- Moves signal masks into kernel process/thread state, adds targeted pending
  signal consumption for `sigwait`, applies stored `posix_spawn` signal
  mask/default attributes through exec, and exposes common signal constants for
  source-compatibility probes.
- Queues observable `SIGCHLD` to active parents when detached children become
  reapable, with default unblocked `SIGCHLD` remaining non-terminating and
  `/fat/bin/posixdemo` covering blocked `SIGCHLD` plus `sigwait`/`waitpid`.
- Adds cooperative libc dispatch for unblocked caught pending signals at
  `waitpid`, `poll`/`select`, sleep, and `sched_yield` boundaries, extends
  `/fat/bin/posixdemo` with caught `SIGCHLD` handler coverage, and lets the
  srvros libuv adapter watch `SIGCHLD`.
- Broadens the catchable signal set to include `SIGQUIT`, `SIGUSR1`, `SIGUSR2`,
  `SIGPIPE`, and `SIGALRM` alongside the existing `SIGINT`/`SIGTERM`/`SIGCHLD`
  path, with POSIX and libuv smoke coverage for `SIGUSR1`.
- Adds `sigsuspend`, distinct interrupted wait/sleep kernel results, libc
  `EINTR` mapping for interrupted `waitpid`, `poll`/`select`, and sleeps, and
  `/fat/bin/posixdemo` coverage for `sigsuspend`, `poll`/`waitpid` interruption,
  and burst `SIGCHLD` child reaping.
- Adds a shared `SRV_ERR_INTR` syscall result and extends caught-signal
  interruption across blocking pipe I/O, console input, Unix-domain accept,
  TCP accept/connect/read/write, UDP receive, and blocking file locks, with
  libc `EINTR` mapping and `/fat/bin/posixdemo` coverage for pipe read and
  Unix-domain accept interruption.
- Honors `SA_RESTART` in libc for restartable blocking calls including fd I/O,
  `waitpid`, blocking file locks, and socket accept/connect/receive paths, with
  `/fat/bin/posixdemo` coverage for a restarted pipe read after a caught signal.
- Expands `srvsh` with `$VAR`/`${VAR}` expansion, `$?`, `$$`, and `&&`/`||`
  command chaining.
- Extends shell parameter expansion with `${VAR:-word}`, `${VAR:=word}`,
  `${VAR:+word}`, `${VAR:?word}`, `${#VAR}`, and prefix/suffix trim operators.
- Adds shell-side unquoted `*`/`?` globbing plus `test`/`[` builtins for string,
  integer, and file/directory checks.
- Adds non-interactive `srvsh` entry points through `sh -c command` and
  `sh script`, allowing ports and smoke tests to launch scriptable shell work
  without first entering the editable prompt.
- Adds `$(command)` command substitution, including quoted and nested forms,
  by capturing stdout through a short-lived shell temp file.
- Adds `if`/`then`/`else`/`fi` control flow for one-line commands and multiline
  script blocks.
- Adds more CLI table-stakes shell behavior: positional parameters
  (`$0`, `$1`, `$#`, `$@`) for scripts and `sh -c`, `set -e`/`set +e`,
  `read VAR`, `unset`, `alias`, `type`, `export NAME`, bare `NAME=value`
  assignments, and `cd -` with directory validation.
- Adds shell stdin redirection plus stderr `2>`/`2>>`/`2>&1` redirection, and moves
  external shell launches onto an `execve`-shaped native request that carries
  argv, envp, background state, and stdin/stdout/stderr fd overrides.
- Extends shell pipelines so each external segment can own stdin/stdout/stderr
  redirections instead of only the first input and last output positions,
  including `2>&1` into a pipe and redirection on non-final segments.
- Adds a shell `exec` builtin that replaces the current shell process with a
  resolved command while honoring the same redirection path.
- Adds POSIX-facing `waitpid`, `posix_spawn`, `posix_spawnp`, standard-stream
  spawn file actions for `dup2`, `open`, and `close`, and true
  process-replacing `execve`.
- Extends spawn file actions with bounded ordered non-stdio `dup2`, `open`,
  and `close` handling, with QEMU coverage through `/fat/bin/fdprobe`.
- Adds first `posix_spawnattr` support: flags/getters/setters plus
  `POSIX_SPAWN_SETPGROUP` mapping onto native process groups.
- Grows POSIX spawn compatibility with dynamically stored non-stdio file
  actions up to the native 32-action limit, signal-set helpers,
  reset-id/signal-mask/default spawn-attribute storage, and stronger
  `/fat/bin/posixdemo` coverage for larger ordered action chains.
- Tightens `scanf`/`sscanf` source-port behavior for width-limited strings,
  `%c`, suppressed conversions, `%n`, and EOF versus match-failure return
  values before the first assignment.
- Adds `/fat/bin/netcheck`, a single guest-side command for DHCP/status, DNS,
  ping, local UDP echo, and outbound TCP HTTP checks.
- Adds `tools/net_soak.py`, a repeated QEMU networking soak that runs host HTTP
  fetches against background `webd` while interleaving guest diagnostics.
- Adds `tools/tcp_pressure.py`, a focused TCP table pressure test that drives
  enough short web connections to exercise `TIME_WAIT` reclaim while confirming
  `netcheck` still passes.
- Raises the kernel TCP connection table to 32 slots so short `TIME_WAIT` bursts
  from low steady web traffic do not starve outbound client connects.
- Extends network status with TCP capacity, `TIME_WAIT`, full-table, reclaim,
  and close-timer counters; `/fat/bin/ifconfig` prints them.
- Reclaims expired or oldest `TIME_WAIT` connections under table pressure and
  moves the enlarged network status syscall snapshot off the kernel stack.
- Adds version/size headers to the network list/status/ARP structs and bounds
  kernel copy-back by the caller-declared size, with `/fat/bin/netabi` and
  `tools/netabi_smoke.py` covering truncated-struct compatibility.
- Extends the same size-versioned ABI pattern to core structured outputs:
  process listing, file status, filesystem status, console/gfx info, mouse
  events, and GUI messages. `/fat/bin/sysabi` plus `tools/sysabi_smoke.py`
  cover truncated-buffer canary checks for the core path.
- Adds `/fat/bin/execdemo` as smoke coverage for in-place `execve`; it replaces
  itself with `/fat/bin/false`, and the parent observes the replacement image's
  exit status. The companion `/fat/bin/fdprobe` verifies inherited and
  close-on-exec fd behavior.
- Tightens empty-file handling so zero-byte exFAT files are registered,
  truncating a file through an fd creates a real zero-byte file, and later
  writes can grow that file by allocating fresh clusters.
- Expands the generated exFAT image builder to reserve multi-cluster root and
  `/fat/bin` directory tables with explicit overflow checks.
- Moves `/fat/bin/webd` onto the readiness API so a partial client no longer
  blocks another HTTP request from completing.
- Extends the generated `/fat/www` sample site with a nested CSS asset and
  smoke coverage for GET, HEAD, and slow-client behavior.
- Ships `/fat/bin/tap`, a small stream splitter for stdout plus a secondary
  file, and uses it in the pipeline smoke path.
- Includes early GUI/windowing experiments and sample GUI apps.
- Includes QEMU smoke tests for CLI, processes, directories, DHCP, DNS, web
  serving, and filesystem stress.
- Smoke harnesses launch QEMU with bounded guest memory and temporary copies of
  the exFAT disk image so test writes do not mutate the repository image.

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
python3 tools/configure_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/shell_edit_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/thread_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dhcp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/web_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/ports_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/uv_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/metadata_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/lua_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/netabi_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/sysabi_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/net_soak.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 3
python3 tools/tcp_pressure.py --qemu /ucrt64/bin/qemu-system-x86_64 --connections 44
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
- Permission bits are srvros-managed metadata. Writable exFAT mounts persist
  them in `/fat/.srvros/meta`; the format is not native exFAT metadata and the
  recovery path is limited to sidecar temp-file promotion/cleanup rather than a
  full journal.
- `stdio` is deliberately small: enough for early command-line ports, including
  common formatted-output, first-pass scanning with scansets, and simple stream
  buffering plus one-way `popen`/`pclose`, but not a full ISO C implementation.
- Lua uses its normal floating-number profile with `math` enabled. The `os`
  library and native dynamic loading remain disabled.
- `/fat/bin/uvdemo` adds the first srvros `uv.h` compatibility shim, covering a
  libuv-shaped loop, timers, filesystem requests, `uv_async_t`,
  pthread-backed `uv_queue_work`, `uv_poll_t`, UDP, and a two-client
  host-forwarded TCP accept/read/write smoke path for the Node.js/libuv
  bring-up track.
- Process-exit teardown is non-preemptible while freeing the exiting address
  space, so repeated larger interpreter launches do not leave scheduler context
  pointing at freed page tables.

### Next Release Themes

- Harden exFAT writes with stronger truncate rollback, sync semantics, and
  broader crash recovery testing.
- Add richer userspace networking APIs, readiness primitives, and socket
  option behavior.
- Strengthen UDP/DNS edge cases now that userspace UDP sockets and DNS-over-UDP
  tools are available.
- Add NVMe storage.
- Expand the shell and support library toward a small libc-shaped environment.
- Move GUI windows toward client-owned shared framebuffers.
- Expand floating-point library coverage beyond the current Lua-ready
  floating-number profile.
