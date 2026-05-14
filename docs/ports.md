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
  read-only regular files. Writable regular-file descriptors share offset,
  dirty state, ownership, and last-close writeback.
- `poll` and `select` for regular files, pipes, TCP listeners/connections, and
  standard streams. The first implementation uses readiness checks with
  tick/yield timeouts; a unified fd wait queue is still future work.
- `fcntl` with `F_GETFL`/`F_SETFL` and `O_NONBLOCK` for pipes, writable regular
  files, read-only regular files, and TCP listener/connection fds. Empty
  nonblocking pipes and listeners/connections without ready data return
  `EAGAIN`/`EWOULDBLOCK`.
- `access`, `isatty`, `fsync`, `truncate`, `ftruncate`, `chmod`, `fchmod`, and
  `umask`. Permission/mode APIs are compatibility shims until srvros grows
  Unix-like file metadata.
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
- `getopt`, `uname`, `atexit`, and an `ENOSYS` `system` stub
- `getpid`
- Minimal `stdio`: `FILE`, standard streams, `fopen`, `fdopen`, `freopen`,
  `fclose`, `fflush`, `fread`, `fwrite`, `fgets`, `fputs`, `fputc`, `getc`,
  `fgetc`, `ungetc`, `fileno`, `ftell`, `fseek`, `rewind`, `fgetpos`,
  `fsetpos`, `remove`, `perror`, `tmpnam`, `tmpfile`, `printf`, `vprintf`,
  `fprintf`, `sprintf`, `snprintf`, and `vsnprintf`
- IPv4 helpers: `htons`, `ntohs`, `htonl`, `ntohl`, `inet_pton`, `inet_ntop`
- TCP-server socket shims for `socket`, `bind`, `listen`, `accept`, `send`,
  `recv`, and `setsockopt`
- `getaddrinfo` backed by the srvros DNS syscall for A records
- newlib-style syscall glue symbols such as `_open`, `_read`, `_write`,
  `_lseek`, `_fstat`, `_stat`, `_sbrk`, `_getpid`, `_gettimeofday`,
  `_unlink`, and `_rename`

The `posixdemo` userspace app exercises the POSIX slice from inside srvros.
The `zlibdemo` app links pinned zlib `v1.3.2`, compresses data, writes the
compressed stream to `/fat`, reads it back, and verifies decompression.
The `/fat/bin/lua` app links pinned Lua `v5.4.8` from a generated srvros build
copy, supports `lua -e <chunk>` and `lua <script.lua>`, and opens the base,
coroutine, table, math, string, UTF-8, debug, IO, and package libraries. It
uses Lua's normal double-number profile backed by srvros `math.h`, `float.h`,
and floating `snprintf` support. `package` is configured for pure Lua modules
under `/fat` and `/fat/lib/lua/5.4`; native C module loading is disabled.

## Current Limits

- `fork`, `execve`, and client-side `connect` are still missing.
- Permission bits are synthetic; `chmod`/`fchmod` validate the target but do not
  persist metadata yet.
- `poll`/`select` cover the current fd types, but timeout waits are currently
  tick/yield based rather than attached to a single scheduler wait queue.
- Read-only regular-file `dup`/`dup2` currently clones a read snapshot and file
  offset instead of sharing one open-file description.
- Socket wrappers currently cover TCP server flow over the existing
  `net_listen`/`net_accept` kernel path. Nonblocking mode is preserved when it
  is set on a socket before `listen()`.
- `stdio` is intentionally small: formatted output has no width/precision
  parsing yet, input scanning is missing, `fflush` is effectively a no-op for
  unbuffered streams, and `popen`/`pclose` are `ENOSYS` stubs.
- Time is tick-derived and not wall-clock accurate.
- The default allocator is process-local and `sbrk` backed; `mmap` is still
  missing.
- The kernel enables FPU/SSE/SSE2 and saves/restores per-process plus
  per-scheduler-thread `fxsave64` state. Kernel C and userspace C both build
  with SSE enabled; only the low-level FPU handoff file is forced no-SSE.
- Lua currently runs in an integer-number profile. Full floating-point Lua is
  now unblocked at the ABI level, but still needs the port configuration and
  missing math-library surface to be switched over.
- Lua excludes `math`, `os`, and dynamic loading for now.

## Upstream Repos

Third-party source is kept as pinned submodules under `ports/upstream`:

- zlib `v1.3.2`
- Lua `v5.4.8`

## Next Porting Milestones

1. Expand `stdio` toward command-line port expectations: width/precision
   formatting, `sscanf`/`fscanf` basics, and better EOF/error state.
2. Move the repeated per-app `_start` assembly into a shared crt startup object
   so every app is built as a single static ELF64 executable from common rules.
3. Add Unix-like file metadata around the current exFAT backend: stable inode
   ids, mode bits, uid/gid placeholders, timestamps, and permission-aware
   `access`.
4. Add `mmap`-style mappings for larger interpreters and libraries.
5. Switch Lua from the integer-number profile toward normal floating numbers
   after adding the remaining math-library support.
6. Move `poll`/`select` timeout waits onto a shared scheduler wait queue and
   expand `webd` toward fuller concurrent connection handling.
7. Add client TCP `connect`, then a tiny HTTP client.
8. Move toward libuv after sockets, timers, and fd readiness are boring.
