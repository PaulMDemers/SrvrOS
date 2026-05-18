# srvros Ports

This directory is the staging area for third-party libraries and applications
that will be brought up on srvros through the POSIX-compat layer.

## Upstream Source

- `upstream/zlib`: zlib compression library, pinned to `v1.3.2`.
- `upstream/lua`: Lua runtime, pinned to `v5.4.8`.
- `upstream/cjson`: cJSON parser/printer, pinned to `v1.7.19`.
- `upstream/inih`: inih INI parser, pinned to `r62`.
- `upstream/linenoise`: linenoise line editing API, pinned to `2.0`.
- `upstream/sqlite`: SQLite amalgamation snapshot, pinned to `3.53.1`.

The zlib core sources are linked directly into `/fat/bin/zlibdemo`, which
compresses data, writes the compressed bytes to exFAT, reads them back, and
verifies decompression inside srvros.

Lua is built as `/fat/bin/lua` from a generated copy under
`build/ports/lua-srvros`, so the upstream submodule stays clean. The current
port uses Lua's normal floating-number profile and disables OS/process/native
dynamic loading features while the libc and kernel process ABI mature.

cJSON and inih are linked directly into `/fat/bin/jsondemo` and
`/fat/bin/inidemo`. These apps verify parse/create/file roundtrips without
patching upstream sources.

linenoise is used through `ports/srvros/linenoise.c`, a small srvros adapter
for the upstream public API. `srvsh` uses it for editable prompt input and
`/fat/.srvsh_history`; `/fat/bin/linedemo` exercises history save/load behavior.

The first libuv-facing bridge lives in `ports/srvros/uv.c` and
`ports/srvros/uv.h`. It is intentionally a small srvros compatibility shim, not
upstream libuv: `/fat/bin/uvdemo` uses it to smoke-test a libuv-shaped loop,
timers, filesystem requests, and initial TCP/UDP handle entry points.

SQLite is kept as an official amalgamation snapshot rather than a submodule.
`/fat/bin/sqlitedemo` builds the amalgamation with `SQLITE_OS_OTHER`, registers
a small srvros VFS, and verifies create/insert/query/reopen behavior on exFAT.
The VFS maps SQLite shared/reserved/pending/exclusive states onto srvros
advisory byte-range locks.

## Porting Rules

- Keep upstream repos clean. Put srvros-specific build glue outside the
  submodule when possible.
- Prefer small compatibility shims in `userspace/lib` over patching upstream.
- Add one smoke app per port once it builds.
- Document unsupported POSIX calls explicitly instead of silently pretending.

## Near-Term Order

1. Expand `stdio` formatting/scanning behavior for more command-line ports.
2. Add Unix-like file metadata over exFAT.
3. Add `mmap`-style mappings for larger ports.
4. Broaden client-side TCP, UDP, socket options, and readiness edge cases.
5. Replace the current srvros `uv.h` shim with a real upstream libuv backend.
