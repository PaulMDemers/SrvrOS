# Porting Plan

srvros is moving toward a POSIX-shaped userspace compatibility layer. The goal is
not to clone Unix in the kernel; the goal is to make portable C libraries see a
familiar API while the kernel keeps a compact srvros syscall ABI.

## Foundation Added

The first compatibility slice now lives under `userspace/lib/include` and
`userspace/lib/src`:

- `errno`
- `open`, `read`, `write`, `close`, `lseek`; `O_RDWR` works for regular files
  through the writable VFS fd path
- `stat`, `fstat`, `mkdir`, `unlink`, `rename`, `rmdir`
- `pipe`; pipes are bounded in-kernel ring buffers with read/write fd endpoints
- `dup` and `dup2` for standard streams, pipes, writable regular files, and
  read-only regular files. Regular-file descriptors share open-file-description
  offsets; writable descriptors also share dirty state, ownership, and
  last-close writeback.
- `poll` and `select` for regular files, pipes, TCP listeners/connections, and
  standard streams. Readiness waits now sleep on a shared fd wait queue with
  timer-backed wakeups for finite timeouts.
- `fcntl` with `F_GETFL`/`F_SETFL` and `O_NONBLOCK` for pipes, writable regular
  files, read-only regular files, and TCP listener/connection fds. Empty
  nonblocking pipes and listeners/connections without ready data return
  `EAGAIN`/`EWOULDBLOCK`.
- `fcntl` descriptor flags with `F_GETFD`/`F_SETFD` and `FD_CLOEXEC` for
  regular fds and POSIX socket pseudo-fds. Process replacement closes marked
  descriptors while preserving explicit stdio redirection fds.
- `fcntl` advisory byte-range locks with `F_GETLK`, `F_SETLK`, and `F_SETLKW`
  for regular files. Locks are process-owned, conflict by pathname and range,
  and are released when that process closes a descriptor for the same path.
- `access`, `isatty`, `fsync`, `truncate`, `ftruncate`, `chmod`, `fchmod`, and
  `umask`. `stat`/`fstat` now expose VFS-managed inode ids, mode bits,
  uid/gid placeholders, block counts, and tick-derived timestamps. Writable
  exFAT mounts persist that metadata through `/fat/.srvros/meta`.
- Minimal `termios`: `tcgetattr` and `tcsetattr` for console fds, with
  canonical/raw input mode toggles, `ICRNL`, `ECHO`, `VMIN`, `VTIME`, erase,
  kill-line, and EOF control characters. `ioctl(TIOCGWINSZ)` reports the
  current console dimensions and framebuffer pixel size, while
  `ioctl(TIOCSWINSZ)` stores an override for terminal-oriented ports.
  Terminal-generated Ctrl-C is routed to the shell's active foreground process
  group; richer terminal-session modeling and baud settings are still future
  work.
- `pread` and `pwrite` for seekable fds. These currently save/restore the file
  offset in userspace around the underlying `lseek` plus `read`/`write`.
- `cat`, `grep`, `head`, and `wc` consume stdin for pipeline-friendly text
  processing
- `getcwd`, `chdir`
- `opendir`, `readdir`, `closedir`
- `malloc`, `calloc`, `realloc`, `free`, `posix_memalign`, and `aligned_alloc`
  backed by `sbrk`-grown heap chunks. The allocator has a process-local
  futex-backed heap lock so same-address-space pthreads can safely share it.
- `brk`/`sbrk` backed by kernel-mapped per-process heap pages
- `atoi`, `atof`, `atol`, `strtod`, `strtof`, `strtol`, `strtoll`,
  `strtoul`, `strtoull`, `abs`, `labs`, `llabs`, `div`, `ldiv`, `lldiv`,
  `rand`, `srand`, `qsort`, and `bsearch`
- process-local `getenv`, `setenv`, `unsetenv`, `putenv`, `clearenv`, and
  `environ`
- `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strchr`, `strrchr`,
  `strpbrk`, `strstr`, `strspn`, `strcspn`, `memchr`, `memcmp`, `strcoll`,
  `strerror`
- `ctype`, C-locale `setlocale`/`localeconv`, `signal` stubs, `assert`,
  `setjmp`/`longjmp`, and integer-safe `math.h` macros
- `time`, `clock_gettime`, `gettimeofday`, `sleep`, `usleep`,
  `nanosleep`, and relative `clock_nanosleep`
- `getpagesize`, `sysconf(_SC_PAGESIZE)`, `sysconf(_SC_NPROCESSORS_ONLN)`,
  and `sysconf(_SC_CLK_TCK)`
- First same-address-space pthread surface: `pthread_create`, `pthread_join`,
  `pthread_detach`, `pthread_exit`, self/equality, basic attributes, stack
  attribute helpers, per-thread user stacks, per-thread TLS keys, mutexes,
  mutex attributes including recursive and error-checking modes, condition
  variables, condition attributes, and `pthread_once`. Threads share the
  process address space and fd table;
  each spawned user thread gets its own scheduler kernel trap stack and user
  FPU state. Detached pthread stacks are reclaimed opportunistically by libc
  when later pthread calls observe completion. Mutexes, condition variables,
  and in-progress `pthread_once` waiters now sleep through srvros futex-style
  wait/wake syscalls instead of spinning.
- `getopt`, `uname`, `atexit`, and `system` backed by `sh -c` through
  `posix_spawnp` plus `waitpid`
- `getpid`
- `waitpid` for srvros background children, plus `posix_spawn`/`posix_spawnp`
  with PATH lookup and standard-fd `posix_spawn_file_actions_adddup2`,
  `posix_spawn_file_actions_addopen`, and `posix_spawn_file_actions_addclose`
  support. Non-stdio spawn file actions are applied as a bounded ordered list
  for `dup2`, `open`, and `close` targets, with a dynamically grown userspace
  action list capped by the native spawn ABI. `posix_spawnattr` supports
  `POSIX_SPAWN_SETPGROUP` through the native process-group field and stores
  reset-id plus signal-mask/default attributes for source compatibility.
- process-replacing `execve` backed by the native argv/envp launch request.
  The new image keeps the same pid and open fd table while replacing the old
  userspace address space, then applies close-on-exec descriptor cleanup.
- Minimal `stdio`: `FILE`, standard streams, `fopen`, `fdopen`, `freopen`,
  `fclose`, `fflush`, `fread`, `fwrite`, `fgets`, `fputs`, `fputc`, `getc`,
  `fgetc`, `ungetc`, `fileno`, `ftell`, `fseek`, `rewind`, `fgetpos`,
  `fsetpos`, `remove`, `perror`, `tmpnam`, `tmpfile`, `setvbuf`, `setbuf`,
  `setlinebuf`, `feof`, `ferror`, `clearerr`, `printf`, `vprintf`,
  `fprintf`, `sprintf`, `snprintf`, `vsnprintf`, `popen`, and `pclose`. It
  now has simple full/line/unbuffered stream buffering, path-backed `fflush`,
  logical positions across read prefetch, common formatted output width,
  precision, padding, sign, alternate-form, length, and `%n` handling, and
  common `scanf`/`sscanf` integer, floating, width, scanset, suppression, `%c`,
  `%n`, and EOF/match-failure return behavior. Streams have recursive futex-backed locks
  exposed through `flockfile`, `ftrylockfile`, and `funlockfile`, so shared
  stdio use from same-address-space pthreads is serialized. `posixdemo` covers
  `w+`, `r+`, and `a+` update streams, including read/write transitions and
  append-after-seek behavior.
- IPv4 helpers: `htons`, `ntohs`, `htonl`, `ntohl`, `inet_pton`, `inet_ntop`
- TCP socket shims for `socket`, `bind`, `listen`, `accept`, `connect`,
  `send`, `recv`, `shutdown`, `setsockopt`, and `getsockopt`
- `getaddrinfo` backed by a userspace UDP DNS query with srvros DNS fallback
- newlib-style syscall glue symbols such as `_open`, `_read`, `_write`,
  `_lseek`, `_fstat`, `_stat`, `_sbrk`, `_getpid`, `_gettimeofday`,
  `_unlink`, and `_rename`
- Initial `mmap`/`munmap`: process-owned anonymous private mappings plus
  eager file-backed `MAP_PRIVATE` mappings from regular fds, with `PROT_READ`,
  `PROT_WRITE`, optional `PROT_EXEC`, `PROT_NONE`, `MAP_ANONYMOUS`, and a
  conservative `MAP_FIXED` path for free ranges. `mprotect` can change
  protection on existing mmap-owned pages, and `msync` validates private
  mmap-owned ranges as a no-op.

The `posixdemo` userspace app exercises the POSIX slice from inside srvros.
It also spawns `/fat/bin/execdemo`, which immediately `execve`s
`/fat/bin/false`; the parent observes exit status 1 to verify real process
image replacement. The same smoke path uses `/fat/bin/fdprobe` to verify both
inherited descriptors and `FD_CLOEXEC` descriptor cleanup across exec.
The `/fat/bin/threadstress` app focuses on preemptive same-process threading:
explicit yields, mutex/condition/once synchronization, TLS keys, pthread
attributes, timed condition waits, recursive/error-checking mutexes, detached
reaping, shared heap allocation, shared regular-file fd writes, and shared
stdio stream writes.
The `zlibdemo` app links pinned zlib `v1.3.2`, compresses data, writes the
compressed stream to `/fat`, reads it back, and verifies decompression.
The `jsondemo` app links pinned cJSON `v1.7.19`, parses a service description,
creates a JSON document, writes it to `/fat`, reads it back, and verifies a
roundtrip parse. The `inidemo` app links pinned inih `r62`, parses config from
memory and from `/fat/inidemo.ini`, and verifies the resulting service fields.
The shell links a srvros-specific linenoise `2.0` adapter that keeps upstream
clean while mapping the public API onto srvros console reads/writes. It supports
editable lines, cursor movement over serial escape sequences, simple completion
hooks, and file-backed history. `/fat/bin/linedemo` verifies history trimming,
save, and load behavior.
The `sqlitedemo` app links the SQLite `3.53.1` amalgamation with
`SQLITE_OS_OTHER` and registers a small srvros VFS. It creates
`/fat/sqlitedemo.db`, inserts two page records, closes and reopens the database,
then verifies prepared query results and the on-disk database size. The VFS maps
SQLite shared/reserved/pending/exclusive transitions onto srvros advisory byte
locks.
The `/fat/bin/ttydemo` app verifies the minimal terminal API by switching stdin
to raw mode, restoring the saved settings, reading and setting window size,
checking duplicated stdio tty identity, and checking that non-terminal file
descriptors report `ENOTTY`.
The `/fat/bin/httpget` app verifies outbound TCP by resolving a host with
`getaddrinfo`, connecting to port 80, sending an HTTP/1.0 request, and printing
the response through the normal socket fd path.
The `/fat/bin/udpdns` app verifies userspace UDP sockets by sending a DNS A
query with `sendto`, waiting with `poll`, and receiving the response with
`recvfrom`. It uses DHCP DNS when available, then `/fat/etc/resolv.conf`, then
QEMU fallback DNS unless an explicit server argument is provided. The
`/fat/bin/host` app verifies the kernel DNS resolver path.
The `/fat/bin/udpecho` app verifies bound UDP sockets and local datagram
delivery with a one-shot server/client echo flow.
The `/fat/bin/netstat` app verifies the kernel network enumeration ABI by
listing TCP listeners, active TCP connections, and UDP sockets with endpoints,
PIDs, queue depth, send availability, and compact TCP lifecycle flags.
The `/fat/bin/ifconfig`, `/fat/bin/route`, and `/fat/bin/arp` apps verify the
kernel network status ABI by listing interface MAC/IP, DHCP route/DNS
configuration, protocol counters, worker counters, and the current ARP
cache. The `/fat/bin/ping` app verifies outbound ICMP echo by resolving a
dotted IPv4 address or DNS name and issuing kernel-backed echo requests.
The `/fat/bin/lua` app links pinned Lua `v5.4.8` from a generated srvros build
copy, supports `lua -e <chunk>`, `lua <script.lua>`, stdin-fed chunks, and a
linenoise-backed interactive REPL when launched from a TTY. It opens the base,
coroutine, table, math, string, UTF-8, debug, IO, and package libraries. It
uses Lua's normal double-number profile backed by srvros `math.h`, `float.h`,
and floating `snprintf` support. `package` is configured for pure Lua modules
under `/fat` and `/fat/lib/lua/5.4`; native C module loading is disabled.
The `/fat/bin/minizip` and `/fat/bin/miniunz` apps link the zlib contrib
MiniZip sources to provide zip archive creation, listing, and extraction.
The `/fat/bin/byacc` app links a pinned Berkeley Yacc snapshot (`t20260126`)
directly into a srvros executable. It uses the upstream C sources plus a small
srvros config header, and the ports smoke test verifies that it can generate
`y.tab.c` and `y.tab.h` from a compact grammar on the exFAT volume.
The `/fat/bin/uvdemo` app links the first srvros `uv.h` compatibility shim. The
shim is not upstream libuv yet; it is a deliberately small bridge that gives
ports a libuv-shaped loop, timers, synchronous filesystem requests, TCP/UDP
handle entry points, `uv_poll_t` fd readiness, `uv_async_t` notifications, and
a pthread-backed `uv_queue_work` path. Its demos cover timer, file I/O,
directory create/remove, rename, async/work callbacks, pipe-backed polling, UDP
echo, a two-client host-forwarded TCP accept/read/write path, and a
guest-outbound TCP client that connects to a host service, queues a deferred
write, drains `uv_stream_get_write_queue_size`, and reads the response. It is
the staging point for replacing the shim with a proper libuv backend as the fd
readiness, thread-pool, signal, TTY, and socket surfaces mature.
Upstream libuv is pinned as a submodule at `ports/upstream/libuv` on tag
`v1.52.1` (`1cfa32f`). `/fat/bin/libuvdemo` is the first dedicated staging
program for that port: it links the srvros adapter and exercises the subset we
want to preserve while swapping in upstream internals, namely timers,
filesystem requests, async notifications, queued work, and generic fd polling.
The version identity functions are already compiled from upstream
`src/version.c`, giving the adapter a concrete upstream object in the link.
The adapter also exposes libuv-style `UV_E*` errno constants plus
`uv_translate_sys_error`, `uv_err_name`, and reentrant string helpers.
Core loop and object parity covers loop data/close helpers, backend
timeout/fd reporting, handle and request type/name/size/data helpers,
active/closing checks, and timer due-in reporting.
TCP stream parity now includes writable-readiness connect completion, deferred
write queues with copied buffers, queued-byte reporting, and close-from-callback
guards so accepted streams can be closed without stale poll-snapshot reads.
Loop parity now includes prepare/check/idle phase handles, poll
disconnect/error mapping with close/restart guards, richer timer repeat helpers,
and queued `uv_getaddrinfo` completions over the srvros POSIX resolver.
The intent is to keep `uvdemo` as broad behavioral coverage while
`libuvdemo` tracks the upstream replacement work.

## Current Limits

- `fork` is still missing, and pthreads are intentionally compact: threads
  share process resources, detached stack cleanup is opportunistic rather than
  timer-driven, and robust cancellation/signal interactions are missing.
- `posix_spawn` file actions currently model the final state of standard fds
  `0`, `1`, and `2`; non-stdio file actions are ordered and dynamically stored
  in userspace, with the native spawn ABI currently capped at 32 actions per
  spawn. Reset-id and signal-mask/default spawn attributes are accepted and
  round-tripped for source compatibility, but reset-id has no visible effect
  until srvros grows a uid/gid model and signal-mask/default application awaits
  fuller userspace signal handling.
- VFS metadata on writable exFAT mounts is persisted through the srvros sidecar
  file `/fat/.srvros/meta`. The metadata is intentionally srvros-specific and
  is not encoded into native exFAT directory entries. Sidecar updates stage
  through `/fat/.srvros/meta.tmp`; mount recovery promotes complete temp files
  and removes malformed temp files before applying metadata.
- `poll`/`select` cover the current fd types and sleep on a shared fd readiness
  wait queue. Timer deadlines wake finite timeouts; broader fd-specific wait
  queues are still future work as the descriptor model grows.
- Socket wrappers cover TCP server flow over `net_listen`/`net_accept`,
  client-side `connect` over `net_connect`, and IPv4 UDP datagrams through
  `sendto`/`recvfrom`. They also expose `getsockname`, `getpeername`,
  kernel-backed TCP `shutdown`, connected UDP shutdown state,
  `setsockopt(SO_REUSEADDR/SO_KEEPALIVE/SO_LINGER/SO_RCVBUF/SO_SNDBUF)`,
  and
  `getsockopt(SO_ERROR/SO_TYPE/SO_ACCEPTCONN/SO_REUSEADDR/SO_KEEPALIVE/SO_LINGER/SO_RCVBUF/SO_SNDBUF)`.
  Nonblocking mode is preserved when it is set on a socket before `listen()` or
  `connect()`, `MSG_DONTWAIT` is accepted on the common send/receive paths, and
  UDP sockets report poll readiness. TCP writes can span multiple payload-sized
  segments from one userspace `write` call, and the kernel keeps a small
  transmit history for timer-based SYN/FIN/data retransmission. TCP writes are
  bounded by a compact outstanding-send window, so `POLLOUT` and nonblocking
  writes now reflect actual transmit availability. Payload sends also honor the
  peer's advertised 16-bit receive window from incoming TCP packets. The kernel
  advertises dynamic receive windows from unread connection-buffer space, sends
  window-update ACKs after userspace reads, and uses a small persist timer for
  zero-window peers. Closed or missing TCP connection tuples now return RSTs so
  failed connects are prompt, and per-connection errors flow back through
  `SO_ERROR`/errno as `ECONNREFUSED`, `ETIMEDOUT`, `ECONNRESET`, or
  `EINPROGRESS`. Window scaling, congestion control, and out-of-order
  receive reassembly are still future work, but duplicate or out-of-order data is now
  ACKed without being appended to the stream twice. TCP close now keeps compact
  FIN-wait/close-wait/last-ack/time-wait states long enough to ACK peer close
  traffic before timer cleanup.
- SQLite is still a compact filesystem smoke port. Its VFS now maps lock states
  to srvros advisory byte-range locks, but richer stale-lock recovery,
  cross-machine semantics, and WAL shared-memory locking are future work.
- The TTY layer is intentionally small. It tracks one console termios state and
  supports canonical/raw reads plus Ctrl-C delivery to the active foreground
  process group, but does not yet model controlling terminals, sessions, or baud
  settings.
- `stdio` is intentionally small: formatted output covers the common
  width/precision/flag forms needed by current ports, input scanning covers
  integers, strings, characters, floating values, scansets, width limits,
  assignment suppression, and common length modifiers, and stream buffering
  covers the simple full/line/unbuffered cases. `popen`/`pclose` support
  one-way `sh -c` pipes, but not bidirectional process streams.
- Time is tick-derived and not wall-clock accurate.
- The default allocator is process-local and `sbrk` backed; `mmap` is eager and
  supports anonymous private mappings plus file-backed private mappings.
  `PROT_NONE` and `mprotect` update page-table permissions, and `msync`
  validates private ranges without writeback. Demand paging, shared mappings,
  writeback, replacement-style `MAP_FIXED`, and signal-delivered page-fault
  recovery are still future work.
- The kernel enables FPU/SSE/SSE2 and saves/restores per-process plus
  per-scheduler-thread `fxsave64` state. Kernel C and userspace C both build
  with SSE enabled; only the low-level FPU handoff file is forced no-SSE.
- Lua now runs its normal floating-number profile with `math` enabled. The `os`
  library and native dynamic loading remain disabled for now.

## Upstream Repos

Third-party source is kept as pinned submodules or snapshots under
`ports/upstream`:

- zlib `v1.3.2`
- libuv `v1.52.1`
- Lua `v5.4.8`
- cJSON `v1.7.19`
- inih `r62`
- linenoise `2.0`
- SQLite amalgamation `3.53.1`
- Berkeley Yacc snapshot `t20260126`

## Next Porting Milestones

1. Expand `stdio` toward command-line port expectations: more ISO C edge cases,
   binary/update-mode corner cases, and broader formatted input behavior.
2. Apply stored `posix_spawn` signal-mask/default attributes once srvros grows
   fuller userspace signal handling; reset-id remains a no-op until a uid/gid
   model exists.
3. Harden the exFAT metadata sidecar further: stronger atomic replacement,
   recovery from broader directory-update failures, and richer timestamp
   sources.
4. Expand `mmap` toward larger interpreters and libraries: demand paging,
   shared mappings where useful, writeback decisions, richer `MAP_FIXED`
   replacement semantics, and signal-delivered page-fault recovery.
5. Expand `webd` toward fuller concurrent connection handling, stronger
   low-rate client behavior, and more complete static-file metadata.
6. Broaden UDP/DNS compatibility now that userspace datagram sockets exist:
   timeout behavior, resolver edge cases, richer `/fat/etc/resolv.conf`
   handling, and better multi-answer/error reporting.
7. Continue the libuv bring-up: replace the srvros shim in layers with a real
   backend once stream lifetimes, nonblocking readiness, timers, filesystem
   requests, TTY, signals, and thread-pool work are boring under smoke tests.
