#!/usr/bin/env python3
"""Run QEMU with a temporary, QEMU-only boot ISO and chosen primary display.

The generated Limine entry uses quiet logging, and adds the VirtIO presenter
only for VirtIO VGA. Native USB/ISO configurations remain untouched. All
arguments following -- are passed directly to QEMU, without a shell.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from displayd_resolution_smoke import build_iso

ROOT = Path(__file__).resolve().parents[1]


def boot_cmdline(display_device):
    if display_device not in ("std", "virtio"):
        raise ValueError("unsupported display device")
    return "srvros.log=quiet" + (" srvros.virtio-gpu=present" if display_device == "virtio" else "")


def qemu_command(qemu, iso, display_device, extra_args):
    video = ["-vga", "none", "-device", "virtio-vga"] if display_device == "virtio" else ["-vga", "std"]
    if display_device not in ("std", "virtio"):
        raise ValueError("unsupported display device")
    return [qemu, "-cdrom", str(iso), *video, *extra_args]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-x86_64")
    parser.add_argument("--display-device", choices=("std", "virtio"), default="virtio")
    parser.add_argument("--virtio-devices", action="store_true", help="enable modern VirtIO input/RNG and kernel drivers; disk/network are supplied after --")
    parser.add_argument("qemu_args", nargs=argparse.REMAINDER, help="QEMU arguments following --")
    args = parser.parse_args(argv)
    extra_args = args.qemu_args
    if extra_args and extra_args[0] == "--":
        extra_args = extra_args[1:]
    with tempfile.TemporaryDirectory(prefix="srvros-qemu-iso-") as directory:
        cmdline=boot_cmdline(args.display_device)
        if args.virtio_devices:
            cmdline+=" srvros.virtio=on"
            extra_args=["-device","virtio-keyboard-pci","-device","virtio-tablet-pci",
                        "-object","rng-random,id=rng0,filename=/dev/urandom",
                        "-device","virtio-rng-pci,rng=rng0,disable-legacy=on",*extra_args]
        iso = build_iso(str(ROOT), directory, 1280, 800, os.environ.copy(), cmdline=cmdline)
        return subprocess.call(qemu_command(args.qemu, iso, args.display_device, extra_args), cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
