# Porting Plan

srvros is moving toward a POSIX-shaped userspace compatibility layer. The goal is
not to clone Unix in the kernel; the goal is to make portable C libraries see a
familiar API while the kernel keeps a compact srvros syscall ABI.

## Foundation Added

The first compatibility slice now lives under `userspace/lib/include` and
`userspace/lib/src`:

- `errno`
- `open`, `read`, `write`, `close`, `lseek`
- `stat`, `mkdir`, `unlink`, `rename`, `rmdir`
- `getcwd`, `chdir`
- `opendir`, `readdir`, `closedir`
- `malloc`, `calloc`, `realloc`, `free`
- `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strchr`, `strrchr`,
  `strpbrk`, `strstr`, `strspn`, `strcspn`, `memchr`, `memcmp`, `strcoll`,
  `strerror`
- `ctype`, C-locale `setlocale`/`localeconv`, `signal` stubs, `assert`,
  `setjmp`/`longjmp`, and integer-safe `math.h` macros
- `time`, `clock_gettime`, `gettimeofday`, `sleep`, `usleep`
- `getpid`
- Minimal `stdio`: `FILE`, standard streams, `fopen`, `fdopen`, `freopen`,
  `fclose`, `fflush`, `fread`, `fwrite`, `fgets`, `fputs`, `fputc`, `getc`,
  `printf`, `vprintf`, `fprintf`, `sprintf`, `snprintf`, and `vsnprintf`
- IPv4 helpers: `htons`, `ntohs`, `htonl`, `ntohl`, `inet_pton`, `inet_ntop`
- TCP-server socket shims for `socket`, `bind`, `listen`, `accept`, `send`,
  `recv`, and `setsockopt`
- `getaddrinfo` backed by the srvros DNS syscall for A records

The `posixdemo` userspace app exercises the POSIX slice from inside srvros.
The `zlibdemo` app links pinned zlib `v1.3.2`, compresses data, writes the
compressed stream to `/fat`, reads it back, and verifies decompression.
The `/fat/bin/lua` app links pinned Lua `v5.4.8` from a generated srvros build
copy, supports `lua -e <chunk>` and `lua <script.lua>`, and opens the base,
coroutine, table, string, UTF-8, and debug libraries.

## Current Limits

- `O_RDWR` is not implemented yet because current writable VFS fds are
  write-buffer fds.
- `fstat`, `pipe`, `dup`, `fork`, `execve`, `poll`, `select`, and client-side
  `connect` are still missing.
- Socket wrappers currently cover TCP server flow over the existing
  `net_listen`/`net_accept` kernel path.
- `stdio` is intentionally minimal: formatted output has no width/precision
  parsing yet, input scanning is missing, and seek/tell support is incomplete.
- Time is tick-derived and not wall-clock accurate.
- The allocator is process-local and static; larger ports will want `brk` or
  `mmap`.
- Lua currently runs in an integer-number profile. Full floating-point Lua
  needs kernel FPU/SSE context save/restore before user processes can safely use
  the x86_64 floating-point ABI.
- Lua excludes `math`, `io`, `os`, `package`, and dynamic loading for now.
- Repeated large Lua process launches in one boot have exposed a kernel heap
  stability issue; the smoke test verifies one script execution per fresh boot
  while that kernel-side cleanup is pending.

## Upstream Repos

Third-party source is kept as pinned submodules under `ports/upstream`:

- zlib `v1.3.2`
- Lua `v5.4.8`

## Next Porting Milestones

1. Expand `stdio` toward command-line port expectations: width/precision
   formatting, `sscanf`/`fscanf` basics, `fseek`/`ftell`, and better EOF/error
   state.
2. Add `brk`/`mmap`-style heap growth for larger interpreters and libraries.
3. Add FPU/SSE process context so Lua can use its normal floating-number
   profile.
4. Add `poll` or `select` over process fds and network wait queues.
5. Add client TCP `connect`, then a tiny HTTP client.
6. Move toward libuv after sockets, timers, and fd readiness are boring.
