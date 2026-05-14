# Executable Format

srvros currently runs statically linked ELF64 executables. The kernel loader
maps `PT_LOAD` segments, creates a user stack, jumps to the ELF entry point, and
passes a compact argc/argv block for programs launched with arguments.

## Near-Term Direction

Keep ELF64 as the native executable container. It already gives us:

- self-contained static binaries,
- standard program headers,
- a clear entry point,
- section/debug metadata for host-side tools, and
- a direct path to existing C toolchains.

The cleanup target is not a new binary format yet; it is a proper userspace
runtime startup object:

- Move the repeated per-app `start.S` code into `userspace/lib/crt0.S`.
- Link every app against `crt0.o` automatically from the common Makefile rules.
- Keep application directories to C/source assets only unless they need custom
  assembly for their own behavior.
- Provide a normal `_start` that calls `main(argc, argv, environ)` and exits via
  the srvros syscall ABI.

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
