# srvros Ports

This directory is the staging area for third-party libraries and applications
that will be brought up on srvros through the POSIX-compat layer.

## Upstream Submodules

- `upstream/zlib`: zlib compression library, pinned to `v1.3.2`.
- `upstream/lua`: Lua runtime, pinned to `v5.4.8`.

These are intentionally not wired into the OS image yet. The first goal is to
keep upstream source snapshots available and pinned while the srvros libc/POSIX
surface grows enough to build them without invasive downstream edits.

## Porting Rules

- Keep upstream repos clean. Put srvros-specific build glue outside the
  submodule when possible.
- Prefer small compatibility shims in `userspace/lib` over patching upstream.
- Add one smoke app per port once it builds.
- Document unsupported POSIX calls explicitly instead of silently pretending.

## Near-Term Order

1. Build zlib as a static userspace object and add a tiny compression smoke app.
2. Build Lua without dynamic loading or OS process features.
3. Add missing `stdio` and `setjmp` pieces discovered by those builds.
4. Add more socket/readiness APIs before moving to libuv.
