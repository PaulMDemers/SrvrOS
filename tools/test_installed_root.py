#!/usr/bin/env python3
"""Build and run the isolated x86_64 host selector regression test."""
import argparse
import os
from pathlib import Path
import platform
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true", help="enable ASan and UBSan")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    out = root / "build/review/installed-root-host"
    out.mkdir(parents=True, exist_ok=True)
    binary = out / ("test_installed_root_sanitize" if args.sanitize else "test_installed_root")
    cmd = shlex.split(os.environ.get("CC", "cc"))
    if not cmd:
        parser.error("CC must name a compiler")
    if platform.system() == "Darwin":
        cmd += ["-arch", "x86_64"]
    if args.sanitize:
        cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    cmd += ["-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root / "kernel/include"),
            str(root / "tools/test_installed_root.c"), str(root / "kernel/src/block_claim.c"),
            "-o", str(binary)]
    subprocess.run(cmd, check=True, cwd=root)
    subprocess.run([str(binary)], check=True, cwd=root)


if __name__ == "__main__":
    main()
