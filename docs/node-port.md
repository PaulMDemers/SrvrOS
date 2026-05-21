# Node.js Port Notes

srvros now carries Node.js as a pinned upstream source checkout:

```text
ports/upstream/node -> v24.16.0 LTS "Krypton" (c7d10158)
```

The first probe used the current LTS release instead of the newest odd/current
line, because LTS gives the port a less volatile target while libc, POSIX, V8,
and libuv support are still moving.

## Configure Probe

Node's configure script does not currently recognize srvros as a destination
OS. A direct configure attempt fails before generating a build graph:

```text
python configure.py --dest-os=srvros --dest-cpu=x64 --cross-compiling ...
configure.py: error: argument --dest-os: invalid choice: 'srvros'
```

A deliberately reduced Linux stand-in configure does complete:

```text
python configure.py --dest-os=linux --dest-cpu=x64 --cross-compiling \
  --fully-static --without-npm --without-corepack --without-ssl \
  --without-sqlite --without-inspector --without-intl \
  --without-node-snapshot --without-node-code-cache --v8-lite-mode --ninja
```

That means the first porting wall is build-system OS registration, not an
immediate configure-time dependency failure. The stand-in profile is only a
probe; srvros should get its own OS flavor so Linux-only assumptions do not
leak into the runtime.

## Initial Minimal Profile

The first runnable target should be a small, static CLI Node:

- `--dest-cpu=x64`
- `--dest-os=srvros`
- `--cross-compiling`
- `--fully-static`
- `--without-npm`
- `--without-corepack`
- `--without-ssl`
- `--without-sqlite`
- `--without-inspector`
- `--without-intl`
- `--without-node-snapshot`
- `--without-node-code-cache`
- `--v8-lite-mode`
- `--ninja`

This avoids npm, TLS, inspector, ICU, SQLite, and snapshot generation until the
base executable, event loop, filesystem, console, timers, and TCP path are
stable.

## Early API Surface Node Uses

The upstream tree makes the expected platform demands for a real Node port:

- V8 executable memory: anonymous and file-backed `mmap`, `mprotect`,
  executable mappings, and `MAP_FIXED`-style replacement behavior.
- Threads and TLS: `pthread_create`, joins, mutexes, condition variables, TLS
  keys, stack inspection, and signal masking around runtime threads.
- libuv: fd readiness, timers, async work, child process plumbing, TCP/UDP,
  pipes, TTY, filesystem requests, signals, and platform info.
- Process/runtime: `getpid`, cwd/chdir, environment, argv, exit status,
  signal handling, and spawn/stdio inheritance.
- Filesystem: `stat`, `lstat`, `fstat`, `realpath`, directory scanning,
  `readlink`/symlink behavior eventually, temp files, rename/unlink/rmdir, and
  robust open flags.
- Networking: nonblocking sockets, `getsockname`/`getpeername`, socket
  options, DNS, TCP connect/listen/accept/read/write/close, and UDP send/recv.
- Dynamic loading and diagnostics: `dlopen`/`dlsym` for native addons and
  `execinfo.h`/`backtrace` for debug paths. These can be stubbed or disabled
  for the first static CLI milestone.

## Next Porting Steps

1. Add `srvros` to Node's configure/GYP OS list in a local patch queue.
2. Map the first `srvros` build profile near the POSIX/Linux/OpenHarmony
   branches while auditing every Linux-specific syscall assumption.
3. Decide whether the first milestone links against the existing srvros libuv
   adapter or starts replacing it with an upstream libuv srvros backend.
4. Add a `nodeprobe` build target that compiles only the platform probe layer
   before attempting the full V8 and Node executable.
5. Keep upstream clean: carry srvros-specific patches or generated build glue
   outside `ports/upstream/node` until a patch queue format is chosen.
