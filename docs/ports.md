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
  backed by `sbrk`-grown heap chunks
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
- `time`, `clock_gettime`, `gettimeofday`, `sleep`, `usleep`
- `getopt`, `uname`, `atexit`, and `system` backed by `sh -c` through
  `posix_spawnp` plus `waitpid`
- `getpid`
- `waitpid` for srvros background children, plus `posix_spawn`/`posix_spawnp`
  with PATH lookup and basic `posix_spawn_file_actions_adddup2` support for
  standard fd remapping
- process-replacing `execve` backed by the native argv/envp launch request.
  The new image keeps the same pid and open fd table while replacing the old
  userspace address space, then applies close-on-exec descriptor cleanup.
- Minimal `stdio`: `FILE`, standard streams, `fopen`, `fdopen`, `freopen`,
  `fclose`, `fflush`, `fread`, `fwrite`, `fgets`, `fputs`, `fputc`, `getc`,
  `fgetc`, `ungetc`, `fileno`, `ftell`, `fseek`, `rewind`, `fgetpos`,
  `fsetpos`, `remove`, `perror`, `tmpnam`, `tmpfile`, `setvbuf`, `setbuf`,
  `setlinebuf`, `feof`, `ferror`, `clearerr`, `printf`, `vprintf`,
  `fprintf`, `sprintf`, `snprintf`, and `vsnprintf`. It now has simple
  full/line/unbuffered stream buffering, path-backed `fflush`, logical
  positions across read prefetch, and common formatted output width,
  precision, padding, sign, alternate-form, length, and `%n` handling.
- IPv4 helpers: `htons`, `ntohs`, `htonl`, `ntohl`, `inet_pton`, `inet_ntop`
- TCP-server socket shims for `socket`, `bind`, `listen`, `accept`, `send`,
  `recv`, and `setsockopt`
- `getaddrinfo` backed by the srvros DNS syscall for A records
- newlib-style syscall glue symbols such as `_open`, `_read`, `_write`,
  `_lseek`, `_fstat`, `_stat`, `_sbrk`, `_getpid`, `_gettimeofday`,
  `_unlink`, and `_rename`

The `posixdemo` userspace app exercises the POSIX slice from inside srvros.
It also spawns `/fat/bin/execdemo`, which immediately `execve`s
`/fat/bin/false`; the parent observes exit status 1 to verify real process
image replacement. The same smoke path uses `/fat/bin/fdprobe` to verify both
inherited descriptors and `FD_CLOEXEC` descriptor cleanup across exec.
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
The `/fat/bin/lua` app links pinned Lua `v5.4.8` from a generated srvros build
copy, supports `lua -e <chunk>` and `lua <script.lua>`, and opens the base,
coroutine, table, math, string, UTF-8, debug, IO, and package libraries. It
uses Lua's normal double-number profile backed by srvros `math.h`, `float.h`,
and floating `snprintf` support. `package` is configured for pure Lua modules
under `/fat` and `/fat/lib/lua/5.4`; native C module loading is disabled.

## Current Limits

- `fork` and client-side `connect` are still missing.
- VFS metadata on writable exFAT mounts is persisted through the srvros sidecar
  file `/fat/.srvros/meta`. The metadata is intentionally srvros-specific and
  is not encoded into native exFAT directory entries. Sidecar updates stage
  through `/fat/.srvros/meta.tmp`; mount recovery promotes complete temp files
  and removes malformed temp files before applying metadata.
- `poll`/`select` cover the current fd types and sleep on a shared fd readiness
  wait queue. Timer deadlines wake finite timeouts; broader fd-specific wait
  queues are still future work as the descriptor model grows.
- Socket wrappers currently cover TCP server flow over the existing
  `net_listen`/`net_accept` kernel path. Nonblocking mode is preserved when it
  is set on a socket before `listen()`.
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
  covers the simple full/line/unbuffered cases, but `popen`/`pclose` are still
  `ENOSYS` stubs.
- Time is tick-derived and not wall-clock accurate.
- The default allocator is process-local and `sbrk` backed; `mmap` is still
  missing.
- The kernel enables FPU/SSE/SSE2 and saves/restores per-process plus
  per-scheduler-thread `fxsave64` state. Kernel C and userspace C both build
  with SSE enabled; only the low-level FPU handoff file is forced no-SSE.
- Lua now runs its normal floating-number profile with `math` enabled. The `os`
  library and native dynamic loading remain disabled for now.

## Upstream Repos

Third-party source is kept as pinned submodules or snapshots under
`ports/upstream`:

- zlib `v1.3.2`
- Lua `v5.4.8`
- cJSON `v1.7.19`
- inih `r62`
- linenoise `2.0`
- SQLite amalgamation `3.53.1`

## Next Porting Milestones

1. Expand `stdio` toward command-line port expectations: broader input matching
   edge cases, append/update-mode polish, and `popen`/`pclose`.
2. Harden the exFAT metadata sidecar further: stronger atomic replacement,
   recovery from broader directory-update failures, and richer timestamp
   sources.
3. Add `mmap`-style mappings for larger interpreters and libraries.
4. Expand `webd` toward fuller concurrent connection handling.
5. Add client TCP `connect`, then a tiny HTTP client.
6. Move toward libuv after sockets, timers, and fd readiness are boring.
