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
rejects the MSYS/Cygwin host environment.

The WSL/Linux probe now gets past configure, Ninja graph generation, V8
inspector/Torque generated-file collisions, libuv's first GNU feature visibility
issue, and V8's host `mksnapshot` link. The build is slow from the Windows
filesystem, but it is no longer blocked at configure, early graph generation,
libuv archive generation, or the first major V8 host generator.

Focused WSL-native probes now confirm that both target and host upstream libuv
archives build with the current srvros patch queue:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget libuv
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget libuv-host
```

The `mksnapshot` milestone now builds with:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget mksnapshot
```

The last blocker there was V8's `v8_libbase` source selection: srvros now maps
onto the POSIX/Linux base sources for `platform-linux.cc` and
`stack_trace_posix.cc`, which provide the host OS and debug helpers needed by
the generator link.

There are now two probe runners:

- `ports/srvros/node/probe.sh`: MSYS-side discovery probe. This verifies the
  patch still reaches V8 compilation, but it is expected to stop at Abseil's
  Cygwin rejection.
- `ports/srvros/node/probe-linux.sh`: Linux/WSL-oriented probe using the same
  reduced Node profile. This is the better path for the next upstream compile
  failure because it avoids MSYS/Cygwin platform distortion. It defaults to
  `NINJA_FLAGS=-j2` so V8 does not overwhelm the host while probing. Override
  `NODE_PROBE_TARGET` to build a smaller target such as `libuv`, `libuv-host`,
  `v8-base`, `mksnapshot`, or `node`.
- `ports/srvros/node/probe-libuv-linux.sh`: focused Linux/WSL probe for the
  target-side upstream libuv archive.
- `ports/srvros/node/probe-mksnapshot-linux.sh`: focused Linux/WSL probe for
  V8's host `mksnapshot` executable, the first major V8 runtime generator.
- `ports/srvros/node/probe-wsl.ps1`: Windows helper that downloads a pinned
  local Linux Ninja into `build/wsl-tools` if needed, then launches the Linux
  probe inside WSL. Pass `-ProbeTarget libuv`, `-ProbeTarget mksnapshot`, or
  another `NODE_PROBE_TARGET` value for focused runs.
- `ports/srvros/node/probe-wsl-native.ps1`: Windows helper that mirrors the
  workspace into `~/srvros-node-probe` and runs the Linux probe from WSL's
  native filesystem. Use this for long V8 builds. It supports the same
  `-ProbeTarget` parameter.

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
`realpath`, pthread attrs/TLS/once/condition variables, `pthread_getattr_np`,
`sysconf`, `getrlimit`, `getrusage`, `sysinfo`, `getauxval`, CPU affinity
stubs, `madvise`, `prctl`, basic `syscall` dispatch, `socketpair`, numeric
`getaddrinfo`, libuv version linkage, `execinfo` stubs, and static-first
`dlfcn` stubs.

The WSL probe host is available in the current workspace. Ubuntu did not have
`ninja` installed and `sudo` required an interactive password, so the helper
uses a pinned local Ninja instead of modifying the WSL distro:

```powershell
ports\srvros\node\probe-wsl.ps1
```

For long compile passes, prefer:

```powershell
ports\srvros\node\probe-wsl-native.ps1
```

For example, the fastest upstream libuv check is:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget libuv
```

If Ninja already exists in WSL, the direct path is still:

```sh
ports/srvros/node/apply-patches.sh
ports/srvros/node/probe-linux.sh
```

## Next Porting Steps

1. Run the focused `node` WSL-native target until the static CLI executable
   reaches its next compile or link failure.
2. Replace the temporary libuv Linux-like srvros probe mapping with a real
   srvros backend or a narrower compatibility shim.
3. Map the first `srvros` build profile near the POSIX/Linux/OpenHarmony
   branches while auditing every Linux-specific syscall assumption.
4. Decide whether the first milestone links against the existing srvros libuv
   adapter or starts replacing it with an upstream libuv srvros backend.
5. Add a `nodeprobe` build target that compiles only the platform probe layer
   before attempting the full V8 and Node executable.
6. Keep upstream clean: carry srvros-specific patches or generated build glue
   outside `ports/upstream/node` until a patch queue format is chosen.
