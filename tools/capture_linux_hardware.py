#!/usr/bin/env python3
"""Collect read-only PCI, USB, HID and ACPI evidence without opening input events."""
import argparse
import datetime
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or Path("build/hardware") / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    output.mkdir(parents=True, exist_ok=False)
    errors = []
    for name, command in {
        "kernel.txt": ["uname", "-srmo"],
        "pci.txt": ["lspci", "-nnk"],
        "pci-resources.txt": ["lspci", "-nnvv"],
        "usb.txt": ["lsusb"],
        "usb-tree.txt": ["lsusb", "-t"],
        "apple-usb.txt": ["lsusb", "-v", "-d", "05ac:0291"],
        "disks.txt": ["lsblk", "-o", "NAME,TYPE,SIZE,FSTYPE,MOUNTPOINTS"],
    }.items():
        try:
            result = subprocess.run(command, capture_output=True, timeout=30)
            (output / name).write_bytes(result.stdout + result.stderr)
            if result.returncode:
                errors.append(f"{name}: command exited {result.returncode}")
        except (OSError, subprocess.TimeoutExpired) as error:
            errors.append(f"{name}: {error}")

    def copy(source, name):
        try:
            target = output / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
        except OSError as error:
            errors.append(f"{source}: {error}")

    for name in ("sys_vendor", "product_name", "product_version", "bios_vendor", "bios_version", "bios_date"):
        copy(Path("/sys/class/dmi/id") / name, f"dmi/{name}")
    copy(Path("/proc/bus/input/devices"), "input-devices.txt")
    for device in Path("/sys/bus/hid/devices").glob("*:05AC:0291.*"):
        copy(device / "report_descriptor", f"hid/{device.name}/report_descriptor.bin")
        copy(device / "uevent", f"hid/{device.name}/uevent.txt")
    for table in Path("/sys/firmware/acpi/tables").glob("*"):
        if table.is_file():
            copy(table, f"acpi/{table.name}.dat")
    (output / "capture.json").write_text(json.dumps({
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "missing_or_partial": errors,
        "note": "No input events, serial numbers from DMI, network addresses or firmware settings were requested. USB descriptors may contain device identifiers."
    }, indent=2) + "\n")
    print(f"Captured Linux hardware evidence: {output.resolve()}")
    print(f"{len(errors)} unavailable/partial reads; see capture.json. ACPI usually requires sudo.")


if __name__ == "__main__":
    main()
