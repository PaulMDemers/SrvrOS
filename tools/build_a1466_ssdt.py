#!/usr/bin/env python3
"""Compile the A1466 SPI/topcase SSDT with iASL."""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="Compile the A1466 input SSDT.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--source", default="ports/qemu-a1466/acpi/a1466-input.asl")
    parser.add_argument("--output", default="build/a1466-input.aml")
    parser.add_argument("--iasl", default=os.environ.get("IASL", ""))
    args = parser.parse_args()

    root = Path(args.root).resolve()
    source = Path(args.source)
    if not source.is_absolute():
        source = root / source
    output = Path(args.output)
    if not output.is_absolute():
        output = root / output
    output.parent.mkdir(parents=True, exist_ok=True)
    prefix = output.with_suffix("")

    iasl = args.iasl or shutil.which("iasl")
    if iasl:
        subprocess.check_call([iasl, "-tc", "-p", str(prefix), str(source)])
    elif shutil.which("wsl.exe"):
        source_linux = subprocess.check_output(
            ["wsl.exe", "-e", "wslpath", "-a", str(source)], text=True).strip()
        prefix_linux = subprocess.check_output(
            ["wsl.exe", "-e", "wslpath", "-a", str(prefix)], text=True).strip()
        command = " ".join([
            "iasl", "-tc", "-p", shlex.quote(prefix_linux),
            shlex.quote(source_linux),
        ])
        subprocess.check_call(["wsl.exe", "sh", "-lc", command])
    else:
        raise RuntimeError(
            "iASL was not found; install ACPICA tools or set IASL to the compiler path")

    if not output.is_file():
        raise RuntimeError(f"iASL did not create {output}")
    print(f"a1466-ssdt: ready: {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"a1466-ssdt: error: {error}", file=sys.stderr)
        raise SystemExit(1)
