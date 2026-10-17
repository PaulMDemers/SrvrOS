#!/usr/bin/env python3
"""Build the pinned QEMU A1466 LPSS rehearsal emulator."""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


QEMU_REPOSITORY = "https://gitlab.com/qemu-project/qemu.git"
QEMU_TAG = "v10.2.0"
QEMU_COMMIT = "698104725efad4b29079d857dfdebbd804e34c99"


def run(command, *, cwd=None, env=None):
    print("+", " ".join(shlex.quote(str(part)) for part in command), flush=True)
    subprocess.check_call(command, cwd=cwd, env=env)


def output(command, *, cwd=None, env=None):
    return subprocess.check_output(command, cwd=cwd, env=env, text=True).strip()


def find_bash(explicit):
    candidates = [
        explicit,
        os.environ.get("BASH"),
        r"C:\msys64\usr\bin\bash.exe",
        shutil.which("bash"),
    ]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return os.path.abspath(candidate)
    raise RuntimeError("MSYS2 bash was not found; install MSYS2 UCRT64 or pass --bash")


def apply_patch_once(source, patch):
    check = subprocess.run(
        ["git", "apply", "--check", str(patch)], cwd=source,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if check.returncode == 0:
        run(["git", "apply", str(patch)], cwd=source)
        return

    reverse = subprocess.run(
        ["git", "apply", "--reverse", "--check", str(patch)], cwd=source,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if reverse.returncode != 0:
        raise RuntimeError(f"QEMU patch neither applies nor is already applied: {patch}")


def main():
    parser = argparse.ArgumentParser(
        description="Clone and build the pinned QEMU A1466 LPSS device model.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--bash", default="")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--configure", action="store_true",
        help="Force configure even when build/build.ninja already exists.")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    package = root / "ports" / "qemu-a1466"
    source = root / "build" / "tooling" / "qemu-a1466-src"
    build = source / "build"
    binary_name = "qemu-system-x86_64" + (".exe" if os.name == "nt" else "")
    binary = build / binary_name
    overlay = package / "src" / "a1466-lpss.c"
    patch = package / "patches" / "0001-register-a1466-lpss.patch"

    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", "--depth", "1", "--branch", QEMU_TAG,
             QEMU_REPOSITORY, str(source)])

    head = output(["git", "rev-parse", "HEAD"], cwd=source)
    if head != QEMU_COMMIT:
        raise RuntimeError(
            f"QEMU source is at {head}, expected pinned {QEMU_COMMIT}; "
            "remove build/tooling/qemu-a1466-src to recreate it")

    target_source = source / "hw" / "misc" / "a1466-lpss.c"
    shutil.copy2(overlay, target_source)
    apply_patch_once(source, patch)

    env = os.environ.copy()
    ucrt_bin = r"C:\msys64\ucrt64\bin"
    msys_bin = r"C:\msys64\usr\bin"
    if os.name == "nt":
        env["PATH"] = os.pathsep.join(
            path for path in (ucrt_bin, msys_bin, env.get("PATH", "")) if path)
        env["MSYSTEM"] = "UCRT64"
        env["QEMU_BUNDLE_NO_SYMLINKS"] = "1"

    if args.configure or not (build / "build.ninja").exists():
        bash = find_bash(args.bash)
        source_msys = (output(
            [bash, "-lc", f"cygpath -u {shlex.quote(str(source))}"], env=env)
            if os.name == "nt" else str(source))
        prefix = (root / "build" / "tooling" / "qemu-a1466-install").as_posix()
        configure = (
            ("export PATH=/ucrt64/bin:/usr/bin:$PATH; " if os.name == "nt" else "") +
            f"cd {shlex.quote(source_msys)}; "
            f"./configure --prefix={shlex.quote(prefix)} "
            "--target-list=x86_64-softmmu --disable-docs --disable-werror"
        )
        run([bash, "-lc", configure], env=env)

    ninja = shutil.which("ninja", path=env["PATH"])
    if not ninja:
        raise RuntimeError("ninja was not found in PATH")
    run([ninja, "-C", str(build), f"-j{args.jobs}",
         binary_name], env=env)

    device_help = output([str(binary), "-device", "help"], env=env)
    for device in ("a1466-lpss-spi", "a1466-lpss-dma"):
        if device not in device_help:
            raise RuntimeError(f"custom QEMU is missing device {device}")

    print(f"qemu-a1466: ready: {binary}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"qemu-a1466: error: {error}", file=sys.stderr)
        raise SystemExit(1)
