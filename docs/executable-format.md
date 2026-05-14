# Executable Format

srvros currently runs statically linked ELF64 executables. The kernel loader
maps `PT_LOAD` segments, creates a user stack, jumps to the ELF entry point, and
passes a compact argc/argv block for programs launched with arguments.

## Current Direction

Keep ELF64 as the native executable container. It already gives us:

- self-contained static binaries,
- standard program headers,
- a clear entry point,
- section/debug metadata for host-side tools, and
- a direct path to existing C toolchains.

The cleanup target is not a new binary format yet; it is a proper userspace
runtime startup object. srvros now links all userspace programs through
`userspace/lib/crt0.S`:

- The repeated per-app `start.S` files have been removed.
- Every app links against `crt0.o` automatically from the common Makefile rules.
- Application directories can stay as C/source assets unless they need custom
  assembly for their own behavior.
- The common `_start` calls `main(argc, argv)` using the argc/argv registers
  populated by the kernel loader, then exits via the srvros syscall ABI.

That gives us the single static executable behavior we want while keeping the
format boring and inspectable.

## Later Packaging

If we want an AppImage-like bundle later, layer it above ELF instead of
replacing ELF:

- A `.srvapp` file can be a small manifest plus one ELF executable plus assets.
- The shell/launcher can mount or unpack the bundle into a private app root.
- The kernel can continue to load the inner ELF exactly the same way.

This keeps the kernel loader small while leaving room for mac-app/AppImage style
application packaging in userspace.
