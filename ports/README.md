# srvros Ports

This directory is the staging area for third-party libraries and applications
that will be brought up on srvros through the POSIX-compat layer.

## Upstream Submodules

- `upstream/zlib`: zlib compression library, pinned to `v1.3.2`.
- `upstream/lua`: Lua runtime, pinned to `v5.4.8`.

The zlib core sources are linked directly into `/fat/bin/zlibdemo`, which
compresses data, writes the compressed bytes to exFAT, reads them back, and
verifies decompression inside srvros. Lua is staged but not built yet; it is the
next larger check on the libc/POSIX surface.

## Porting Rules

- Keep upstream repos clean. Put srvros-specific build glue outside the
  submodule when possible.
- Prefer small compatibility shims in `userspace/lib` over patching upstream.
- Add one smoke app per port once it builds.
- Document unsupported POSIX calls explicitly instead of silently pretending.

## Near-Term Order

1. Expand `stdio` and add `setjmp`/`longjmp`.
2. Build Lua without dynamic loading or OS process features.
3. Add `poll`/`select`-style readiness over process file descriptors.
4. Add client-side TCP `connect` and UDP sockets.
5. Add more socket/readiness APIs before moving to libuv.
