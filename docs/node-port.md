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

The first local patch queue lives at:

```text
ports/srvros/node/patches/0001-add-srvros-gyp-configure-probe.patch
```

It adds `srvros` to configure/GYP, maps the first POSIX-ish compiler/linker
branches, fixes MSYS Python/Ninja path handling for the probe, and disables a
few Windows/shared-library assumptions that prevented V8 from reaching normal
source compilation.

After applying that patch, `--dest-os=srvros` configure completes. A
non-cross-compiling host probe reaches V8 compilation and stops when Abseil
rejects the MSYS/Cygwin host environment. A true cross build still needs V8's
host-generated and target-generated files split cleanly.

There are now two probe runners:

- `ports/srvros/node/probe.sh`: MSYS-side discovery probe. This verifies the
  patch still reaches V8 compilation, but it is expected to stop at Abseil's
  Cygwin rejection.
- `ports/srvros/node/probe-linux.sh`: Linux/WSL-oriented probe using the same
  reduced Node profile. This is the better path for the next upstream compile
  failure because it avoids MSYS/Cygwin platform distortion.

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

srvros now also ships `/fat/bin/nodeprobe`, a small userspace readiness probe
that exercises the local pieces Node is likely to lean on before V8 itself is
portable: `clock_gettime`, `getrandom`, anonymous/file-backed `mmap`,
`mprotect`, `msync`, `mkostemp`, `fcntl(F_DUPFD_CLOEXEC)`, `writev`,
`realpath`, pthread attrs/TLS/once/condition variables, `sysconf`,
`getrlimit`, `getrusage`, `socketpair`, numeric `getaddrinfo`, libuv version
linkage, `execinfo` stubs, and static-first `dlfcn` stubs.

## Next Porting Steps

1. Run the patched probe from a Linux host or a real srvros cross
   compiler so Abseil no longer sees Cygwin/MSYS as the target environment.
2. Split V8 host/target generated files cleanly for `--cross-compiling`.
3. Map the first `srvros` build profile near the POSIX/Linux/OpenHarmony
   branches while auditing every Linux-specific syscall assumption.
4. Decide whether the first milestone links against the existing srvros libuv
   adapter or starts replacing it with an upstream libuv srvros backend.
5. Add a `nodeprobe` build target that compiles only the platform probe layer
   before attempting the full V8 and Node executable.
6. Keep upstream clean: carry srvros-specific patches or generated build glue
   outside `ports/upstream/node` until a patch queue format is chosen.
