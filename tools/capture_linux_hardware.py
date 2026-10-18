#!/usr/bin/env python3
"""Collect read-only PCI, USB, HID, ACPI, power and backlight evidence without input capture."""
import argparse
import datetime
import json
from pathlib import Path
import subprocess


def capture_laptop_state(output, errors):
    """Read an explicit non-identifying property set; never alter power/display state."""
    fields = {
        "power_supply": ("type", "status", "online", "present", "capacity",
            "voltage_now", "current_now", "power_now", "energy_now", "energy_full",
            "energy_full_design", "charge_now", "charge_full", "charge_full_design",
            "cycle_count", "technology", "temp"),
        "backlight": ("type", "brightness", "actual_brightness", "max_brightness", "bl_power"),
    }
    captured = {}
    for kind, names in fields.items():
        root = Path("/sys/class") / kind
        devices = []
        if root.is_dir():
            for node in sorted(root.iterdir()):
                item = {"name": node.name, "sysfs_path": str(node.resolve()), "properties": {}}
                for name in names:
                    source = node / name
                    if not source.exists():
                        continue
                    try:
                        item["properties"][name] = source.read_text().strip()
                    except OSError as error:
                        errors.append(f"{source}: {error}")
                device = node / "device"
                item["firmware"] = {}
                for name in ("path", "hid", "modalias"):
                    source = device / name
                    if source.exists():
                        try:
                            item["firmware"][name] = source.read_text().strip()
                        except OSError as error:
                            errors.append(f"{source}: {error}")
                for name, source in (("driver", device / "driver"),
                    ("physical_driver", device / "physical_node" / "driver")):
                    if source.exists():
                        item[name] = str(source.resolve())
                devices.append(item)
        captured[kind] = devices
    captured["note"] = "Read-only Linux sysfs snapshots; values retain kernel units and are not sampled atomically. Battery serial/manufacturer/model fields are excluded."
    (output / "laptop-state.json").write_text(json.dumps(captured, indent=2) + "\n")


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
    capture_laptop_state(output, errors)
    (output / "capture.json").write_text(json.dumps({
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "missing_or_partial": errors,
        "note": "No input events, serial numbers from DMI, network addresses or firmware settings were requested. USB descriptors may contain device identifiers."
    }, indent=2) + "\n")
    print(f"Captured Linux hardware evidence: {output.resolve()}")
    print(f"{len(errors)} unavailable/partial reads; see capture.json. ACPI usually requires sudo.")


if __name__ == "__main__":
    main()
