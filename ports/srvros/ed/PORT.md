# ed

## Current Source

`/fat/bin/ed` is currently a srvros-local compact line editor implemented in
`userspace/ed/ed.c`. It is meant to cover the scriptable editing needs that
show up in configure scripts, Makefiles, package patch steps, and smoke tests
while the libc/POSIX layer grows enough for a fuller upstream editor.

## Upstream Target

The likely next upstream target is `sbase ed` or another small POSIX-oriented
`ed` implementation. Those versions need a real public `regex.h` surface,
`regcomp`/`regexec` behavior, and more signal/editor diagnostics before they
can replace the local implementation cleanly.

## Implemented Surface

- Optional `-s` quiet mode flag.
- Optional startup file load.
- Numeric, `.`, `$`, and comma range addresses.
- `a`, `i`, `c`, `d`, `p`, `e`, `r`, `w`, `q`, `Q`, and no-op `H` commands.
- Scripted stdin operation with `.` line termination for text input commands.
- `/fat/bin/ed` installation in the exFAT image and initramfs.
- `tools/ports_smoke.py` coverage that edits, writes, and compares an output
  file inside QEMU.

## Known Gaps

- Regex addresses, substitution, global commands, marks, joins, and transfers.
- Full POSIX `ed` unsaved-buffer warnings and diagnostic behavior.
- Prompt handling beyond script-friendly quiet operation.
- Very large line and file behavior beyond the current fixed input buffer.
