# Node.js Port Manifest

## Upstream

- Project: Node.js
- Version: `v24.16.0` LTS "Krypton"
- Commit: `c7d10158bc31036de6783d66beaaaf551e3167aa`
- Source: `ports/upstream/node`

## Status

This is an exploratory source staging point. Node has not been built for srvros
yet, but the local patch queue now gets upstream configure past OS selection,
generates a Ninja graph, separates V8 host/target generated files, builds
target and host libuv focused archives, and produces V8's host `mksnapshot`
generator plus a reduced static `node` executable under WSL-native probing.

The first configure probe found that upstream Node does not accept
`--dest-os=srvros`. A reduced `--dest-os=linux` cross-configure completes, so
the first real task is to register srvros as a build-system OS and then drive
the failures down into compile/link/runtime compatibility gaps.

`patches/0001-add-srvros-gyp-configure-probe.patch` is the current local probe
patch. Apply it with:

```sh
ports/srvros/node/apply-patches.sh
```

Then run the host-side probe with:

```sh
ports/srvros/node/probe.sh
```

The probe expects MSYS2 packages `python`, `ninja`, `nasm`, and `gcc`. It uses
MSYS GCC intentionally; UCRT/MinGW GCC leaks Windows platform defines into V8
and hides the POSIX-ish surface we actually want to inspect.

For the next serious upstream compile failure, prefer:

```sh
ports/srvros/node/probe-linux.sh
```

from Linux or WSL after applying the patch queue. That path avoids the current
Abseil/Cygwin host rejection.

On this Windows workspace the WSL helper downloads a pinned local Ninja under
`build/wsl-tools` when Ubuntu does not already have `ninja` installed:

```powershell
ports\srvros\node\probe-wsl.ps1
```

For the full V8 compile, prefer the WSL-native helper. It mirrors the workspace
to `~/srvros-node-probe`, skips generated build output, reuses the same patch
queue, and avoids the `/mnt/c` filesystem bridge:

```powershell
ports\srvros\node\probe-wsl-native.ps1
```

The Linux probe defaults to `NINJA_FLAGS=-j2` to keep long V8 compiles from
overwhelming the host. Override it only when the machine has enough spare CPU
and memory for a full Node build.

For narrower iteration, `probe-linux.sh` also accepts `NODE_PROBE_TARGET`.
Useful aliases are `libuv`, `libuv-host`, `v8-base`, `mksnapshot`, and `node`.
The convenience wrappers `probe-libuv-linux.sh` and
`probe-mksnapshot-linux.sh` exercise the two most useful smaller milestones.
The PowerShell WSL helpers expose the same choice as `-ProbeTarget`.

For the first srvros-native link check, run:

```powershell
ports\srvros\node\probe-srvros-toolchain.ps1
```

This exports `build/sysroot/srvros`, then builds a tiny freestanding C/C++
program against `crt0.o`, `app.ld`, and `libsrvros.a`. It does not compile Node
itself yet; it verifies the local ABI package that the Node cross-link will use.

To replay the generated Node final link against that sysroot, run:

```powershell
ports\srvros\node\probe-srvros-link.ps1
```

This expects the WSL-native `node` probe to have already generated and built the
Node object graph. It writes the raw link output and a deduplicated unresolved
symbol list under `build/node-srvros-link-probe`.

The srvros image also includes `/fat/bin/nodeprobe`, a small local readiness
probe for the libc/POSIX/libuv surface Node needs before the full V8 build is
worth iterating on, including resource/accounting calls such as `getrlimit` and
`getrusage`, Linux-ish discovery calls such as `getauxval`, `sysinfo`, `prctl`,
`madvise`, and scheduler affinity stubs.

The Windows workspace can enter Ubuntu/WSL at `/mnt/c/Users/Paul/Desktop/srvros`.
The mounted-workspace WSL probe reached late V8/Node compilation. It is slow on
the Windows filesystem, so the WSL-native helper is now the preferred path. The
last concrete build blockers fixed were:

- Ninja duplicate outputs from host and target V8 generated files.
- libuv's Linux/GNU feature branch not being selected for the srvros probe.
- Target and host libuv focused probes now build successfully.
- V8 `v8_libbase` omitting POSIX/Linux platform and stack-trace sources for the
  srvros OS flavor; `mksnapshot` now links and answers `--help`.
- c-ares not selecting a generated config header for srvros.
- QUIC metadata version includes not respecting the no-OpenSSL/no-QUIC reduced
  profile.
- The reduced static `node` WSL-host probe now links and reports platform
  `srvros` and arch `x64`.

## First Target

Build a static CLI-only Node executable without npm, corepack, TLS, inspector,
ICU, SQLite, snapshots, or the code cache. That keeps the first milestone aimed
at the runtime core: V8 startup, libuv event loop, filesystem, stdio, timers,
and TCP/UDP.

## Known Gaps

- Node/GYP/V8 OS flavor for `srvros`.
- C++ runtime and exception/unwind expectations for the Node/V8 build.
- V8 executable memory support through `mmap`/`mprotect`.
- Fuller pthread stack/TLS/signal behavior.
- Real upstream libuv backend or a compatibility bridge broad enough for Node.
- Diagnostic stubs for backtrace/debug paths.
- Static-first policy for native addons until `dlopen` exists.
- More targeted srvros libuv backend work instead of the temporary
  Linux-like probe mapping.
- Real srvros cross-linking instead of the current WSL-host static probe.
- Replacing glibc-only static-link dependencies such as passwd/group/service
  lookup and dynamic-loader paths with srvros libc/POSIX behavior.
- Wiring Node's generated Ninja/GYP link command to the exported srvros sysroot
  and recording the first true srvros-native Node unresolved symbols.
- C++ runtime/ABI support for Node/V8's no-exception C++ build, plus replacing
  host-glibc-built Node objects with srvros-compiled objects.
- The first C++ runtime/ABI slice is in `libsrvros.a`, so the next link frontier
  is mostly host-glibc aliases, math/wide-char gaps, and libuv/Linux backend
  APIs rather than `operator new/delete` or `__cxa_guard_*`.
- Abseil/V8 host-probe behavior: MSYS/Cygwin is useful for discovery but
  Abseil rejects it, so the next serious compile probe should use a Linux host
  or the eventual srvros cross compiler rather than treating MSYS as the final
  build environment.
