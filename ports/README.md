# srvros Ports

This directory is the staging area for third-party libraries and applications
that will be brought up on srvros through the POSIX-compat layer.

## Upstream Submodules

- `upstream/zlib`: zlib compression library, pinned to `v1.3.2`.
- `upstream/lua`: Lua runtime, pinned to `v5.4.8`.
- `upstream/cjson`: cJSON parser/printer, pinned to `v1.7.19`.
- `upstream/inih`: inih INI parser, pinned to `r62`.
- `upstream/linenoise`: linenoise line editing API, pinned to `2.0`.

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
4. Add client-side TCP `connect` and UDP sockets.
5. Add more socket/readiness APIs before moving to libuv.
