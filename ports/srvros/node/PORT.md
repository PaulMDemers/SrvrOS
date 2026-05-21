# Node.js Port Manifest

## Upstream

- Project: Node.js
- Version: `v24.16.0` LTS "Krypton"
- Commit: `c7d10158bc31036de6783d66beaaaf551e3167aa`
- Source: `ports/upstream/node`

## Status

This is an exploratory source staging point. Node has not been built for srvros
yet, but the first local patch queue now gets upstream configure past OS
selection and into V8 compilation.

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

The srvros image also includes `/fat/bin/nodeprobe`, a small local readiness
probe for the libc/POSIX/libuv surface Node needs before the full V8 build is
worth iterating on.

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
- Cross-build host/target generated-file separation for V8.
- Abseil/V8 host-probe behavior: MSYS/Cygwin is useful for discovery but
  Abseil rejects it, so the next serious compile probe should use a Linux host
  or the eventual srvros cross compiler rather than treating MSYS as the final
  build environment.
