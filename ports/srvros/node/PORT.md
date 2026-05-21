# Node.js Port Manifest

## Upstream

- Project: Node.js
- Version: `v24.16.0` LTS "Krypton"
- Commit: `c7d10158bc31036de6783d66beaaaf551e3167aa`
- Source: `ports/upstream/node`

## Status

This is an exploratory source staging point. Node has not been patched or built
for srvros yet.

The first configure probe found that upstream Node does not accept
`--dest-os=srvros`. A reduced `--dest-os=linux` cross-configure completes, so
the first real task is to register srvros as a build-system OS and then drive
the failures down into compile/link/runtime compatibility gaps.

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
