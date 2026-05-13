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
- IPv4 helpers: `htons`, `ntohs`, `htonl`, `ntohl`, `inet_pton`, `inet_ntop`
- TCP-server socket shims for `socket`, `bind`, `listen`, `accept`, `send`,
  `recv`, and `setsockopt`
- `getaddrinfo` backed by the srvros DNS syscall for A records

The `posixdemo` userspace app exercises the slice from inside srvros.

## Current Limits

- `O_RDWR` is not implemented yet because current writable VFS fds are
  write-buffer fds.
- `fstat`, `pipe`, `dup`, `fork`, `execve`, `poll`, `select`, and client-side
  `connect` are still missing.
- Socket wrappers currently cover TCP server flow over the existing
  `net_listen`/`net_accept` kernel path.
- `stdio` is not implemented yet. zlib can start without much of it, but Lua
  will force real `FILE` work or a constrained build.
- Time is tick-derived and not wall-clock accurate.
- The allocator is process-local and static; larger ports will want `brk` or
  `mmap`.

## Upstream Repos

Third-party source is kept as pinned submodules under `ports/upstream`:

- zlib `v1.3.2`
- Lua `v5.4.8`

## Next Porting Milestones

1. Add enough `stdio` for simple command-line ports: `FILE`, `fopen`, `fread`,
   `fwrite`, `fclose`, `fflush`, `fputs`, `fgets`, `fprintf`/minimal formatted
   output.
2. Add `setjmp`/`longjmp`, which Lua needs.
3. Build zlib into a srvros userspace smoke binary.
4. Build Lua with dynamic loading, subprocesses, and OS-specific calls disabled.
5. Add `poll` or `select` over process fds and network wait queues.
6. Add client TCP `connect`, then a tiny HTTP client.
7. Move toward libuv after sockets, timers, and fd readiness are boring.
