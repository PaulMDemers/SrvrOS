#!/usr/bin/env python3
"""Check that the widget archive imports only rendering and pure libc helpers.

This checks link dependencies, not arbitrary C behavior. It complements review:
controls must not allocate, perform I/O, call the OS or run hidden event loops.
"""
import argparse
import subprocess
from pathlib import Path

RENDERER = {
    "desk_clip", "desk_fill", "desk_intersect", "desk_text", "desk_text_width",
    "desk_utf8_next", "desk_text_block_draw", "desk_text_block_height",
    "desk_font_16", "desk_font_20", "desk_font_mono_16", "desk_font_digits_32",
}
PURE_RUNTIME = {
    "memchr", "memcmp", "memcpy", "memmove", "memset", "strchr", "strcmp",
    "strcpy", "strlen", "strncmp", "strrchr", "snprintf", "vsnprintf",
    "__memcpy_chk", "__memmove_chk", "__memset_chk", "__snprintf_chk",
    "__vsnprintf_chk", "__strcpy_chk", "__stack_chk_fail", "__stack_chk_guard",
    "__chkstk_ms", "___chkstk_ms", "__mingw_snprintf", "__mingw_vsnprintf",
}


def symbols(text):
    defined, undefined = set(), set()
    for line in text.splitlines():
        fields = line.split()
        if not fields or line.endswith(":"):
            continue
        if len(fields) < 2 or len(fields[1]) != 1:
            raise ValueError("Unrecognized nm output: " + line)
        name, kind = fields[:2]
        if kind == "U" or (kind in "wv" and len(fields) == 2):
            undefined.add(name)
        else:
            defined.add(name)
    if not defined:
        raise ValueError("Archive contains no global definitions")
    return defined, undefined


def canonical(name):
    # MinGW may import a pure CRT routine through its DLL import pointer.
    return name.removeprefix("__imp_")


def check(archive, nm):
    result = subprocess.run([nm, "-g", "-P", str(archive)],
                            check=True, capture_output=True, text=True)
    defined, undefined = symbols(result.stdout)
    external = undefined - defined
    denied = sorted(name for name in external
                    if canonical(name) not in RENDERER | PURE_RUNTIME)
    if denied:
        raise ValueError(f"{archive}: unexpected widget dependencies: " + ", ".join(denied))
    print(f"widget-boundary: PASS {archive}: {len(defined)} definitions, "
          f"{len(external)} external rendering/pure-runtime helpers; no OS, allocation or I/O imports")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--nm", default="nm", help="GNU/LLVM nm with POSIX output")
    args = parser.parse_args()
    try:
        for archive in args.archives:
            check(archive, args.nm)
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        parser.exit(1, f"widget-boundary: FAIL {error}\n")


if __name__ == "__main__":
    main()
