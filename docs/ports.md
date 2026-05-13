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
  `memcmp`
- `time`, `clock_gettime`, `gettimeofday`, `sleep`, `usleep`
- `getpid`
- Minimal `stdio`: `FILE`, standard streams, `fopen`, `fdopen`, `fclose`,
  `fflush`, `fread`, `fwrite`, `fgets`, `fputs`, `fputc`, `printf`,
  `fprintf`, `snprintf`, and `vsnprintf`
- IPv4 helpers: `htons`, `ntohs`, `htonl`, `ntohl`, `inet_pton`, `inet_ntop`
- TCP-server socket shims for `socket`, `bind`, `listen`, `accept`, `send`,
  `recv`, and `setsockopt`
- `getaddrinfo` backed by the srvros DNS syscall for A records

The `posixdemo` userspace app exercises the POSIX slice from inside srvros.
The `zlibdemo` app links pinned zlib `v1.3.2`, compresses data, writes the
compressed stream to `/fat`, reads it back, and verifies decompression.

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

## Upstream Repos

Third-party source is kept as pinned submodules under `ports/upstream`:

- zlib `v1.3.2`
- Lua `v5.4.8`

## Next Porting Milestones

1. Expand `stdio` toward command-line port expectations: width/precision
   formatting, `sscanf`/`fscanf` basics, `fseek`/`ftell`, and better EOF/error
   state.
2. Add `setjmp`/`longjmp`, which Lua needs.
3. Build Lua with dynamic loading, subprocesses, and OS-specific calls disabled.
4. Add `poll` or `select` over process fds and network wait queues.
5. Add client TCP `connect`, then a tiny HTTP client.
6. Move toward libuv after sockets, timers, and fd readiness are boring.
