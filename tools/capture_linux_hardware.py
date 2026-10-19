#!/usr/bin/env python3
"""Collect read-only PCI, USB, HID, ACPI, power and backlight evidence without input capture."""
import argparse
import datetime
import json
from pathlib import Path
import re
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


def capture_device_details(output, errors, sys_root=Path("/sys"), proc_root=Path("/proc")):
    """Copy descriptor/capability files only; never open an input/event node."""
    def read(path, binary=False):
        try: return path.read_bytes() if binary else path.read_text().strip()
        except OSError as error:
            errors.append(f"{path}: {error}");return None
    usb=[]
    for device in sorted((sys_root/"bus/usb/devices").glob("*")):
        if not (device/"idVendor").is_file(): continue
        item={"path":device.name,"properties":{},"interfaces":[]}
        for name in ("idVendor","idProduct","bcdDevice","bDeviceClass","bDeviceSubClass","bDeviceProtocol","bNumConfigurations","speed","busnum","devnum","devpath"):
            if (device/name).exists(): item["properties"][name]=read(device/name)
        descriptors=read(device/"descriptors",True)
        if descriptors is not None:
            relative=Path("usb")/device.name/"descriptors.bin"
            (output/relative).parent.mkdir(parents=True,exist_ok=True)
            (output/relative).write_bytes(descriptors);item["descriptors"]=str(relative)
        for interface in sorted(device.glob(device.name+":*")):
            entry={"path":interface.name}
            for name in ("bInterfaceNumber","bAlternateSetting","bNumEndpoints","bInterfaceClass","bInterfaceSubClass","bInterfaceProtocol","modalias"):
                if (interface/name).exists():entry[name]=read(interface/name)
            if (interface/"driver").exists():entry["driver"]=(interface/"driver").resolve().name
            item["interfaces"].append(entry)
        usb.append(item)
    sensors=[]
    for node in sorted((sys_root/"class/hwmon").glob("hwmon*")):
        # Older hwmon drivers expose attributes on the parent device.
        source=node if (node/"name").exists() else node/"device"
        item={"name":read(source/"name"),"values":{}}
        for pattern in ("temp*_label","temp*_input","temp*_max","temp*_crit","temp*_crit_alarm","fan*_label","fan*_input","fan*_min","fan*_max"):
            for prop in sorted(source.glob(pattern)):item["values"][prop.name]=read(prop)
        sensors.append(item)
    codecs=[]
    for node in sorted((proc_root/"asound").glob("card*/codec#*")):
        text=read(node)
        if text is not None:
            relative=Path("audio")/node.parent.name/(node.name+".txt")
            (output/relative).parent.mkdir(parents=True,exist_ok=True)
            (output/relative).write_text(text+"\n");codecs.append(str(relative))
    cpu={};info=read(proc_root/"cpuinfo")
    if info:
        for line in info.split("\n\n",1)[0].splitlines():
            key,sep,value=line.partition(":");key=key.strip()
            if sep and key in ("vendor_id","cpu family","model","model name","stepping","microcode","flags"):cpu[key]=value.strip()
    result={"usb":usb,"sensors":sensors,"audio_codec_files":codecs,"boot_cpu":cpu,
        "note":"Read-only descriptors and capabilities. USB string contents, input events, network addresses and sensor-control writes are excluded. Values retain Linux units."}
    (output/"device-details.json").write_text(json.dumps(result,indent=2)+"\n")


def capture_machine_topology(output, errors, sys_root=Path("/sys")):
    """Model-independent capabilities; no event, raw disk or control nodes."""
    def read(path, binary=False):
        try: return path.read_bytes() if binary else path.read_text().strip()
        except OSError as error:
            errors.append(f"{path}: {error}");return None
    def properties(node, names):
        return {name: read(node/name) for name in names if (node/name).is_file()}
    def driver(node):
        return (node/'driver').resolve().name if (node/'driver').exists() else None
    def blob(node, relative):
        data=read(node,True)
        if data is None:return None
        (output/relative).parent.mkdir(parents=True,exist_ok=True)
        (output/relative).write_bytes(data);return str(relative)
    hid=[]
    for node in sorted((sys_root/'bus/hid/devices').glob('*')):
        event=read(node/'uevent')
        if event is None:continue
        fields=dict(line.split('=',1) for line in event.splitlines() if '=' in line)
        identity=re.fullmatch(r'([0-9a-fA-F]+):([0-9a-fA-F]+):([0-9a-fA-F]+)',fields.get('HID_ID',''))
        if not identity or int(identity[2],16)!=0x05ac:continue
        hid.append({'path':node.name,'sysfs_path':str(node.resolve()),'driver':driver(node),
            'hid_id':fields['HID_ID'],'modalias':fields.get('MODALIAS'),
            'report_descriptor':blob(node/'report_descriptor',Path('hid')/node.name/'report_descriptor.bin')})
    graphics=[]
    for node in sorted((sys_root/'class/drm').glob('card*')):
        if re.fullmatch(r'card[0-9]+',node.name):
            graphics.append({'name':node.name,'kind':'card','device_path':str((node/'device').resolve()),
                'driver':driver(node/'device'),'properties':properties(node/'device',
                    ('vendor','device','subsystem_vendor','subsystem_device','class','boot_vga'))})
        elif (node/'status').exists():
            item={'name':node.name,'kind':'connector','properties':properties(node,('status','enabled','modes','dpms'))}
            if (node/'edid').exists():item['edid']=blob(node/'edid',Path('drm')/node.name/'edid.bin')
            graphics.append(item)
    pci=[]
    for node in sorted((sys_root/'bus/pci/devices').glob('*')):
        pci.append({'address':node.name,'sysfs_path':str(node.resolve()),'driver':driver(node),
            'properties':properties(node,('vendor','device','subsystem_vendor','subsystem_device','class','revision','boot_vga'))})
    disks=[]
    for node in sorted((sys_root/'class/block').glob('*')):
        if re.match(r'(?:loop|ram|zram)[0-9]+$',node.name):continue
        disks.append({'name':node.name,'sysfs_path':str(node.resolve()),
            'device_path':str((node/'device').resolve()) if (node/'device').exists() else None,
            'driver':driver(node/'device'),'properties':properties(node,('size','ro','removable','partition','start')),
            'queue':properties(node/'queue',('logical_block_size','physical_block_size','rotational'))})
    result={'apple_hid':hid,'graphics':graphics,'pci':pci,'block':disks,
        'board':properties(sys_root/'class/dmi/id',('board_vendor','board_name','board_version')),
        'note':'Read-only topology and capabilities. No input events, HID unique strings, DMI serials, raw disks, EDID overrides or control writes. Raw EDID may identify a monitor; keep captures local. Missing reads are recorded in capture.json; no provider support is inferred.'}
    (output/'machine-topology.json').write_text(json.dumps(result,indent=2)+'\n')


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
    for table in Path("/sys/firmware/acpi/tables").glob("*"):
        if table.is_file():
            copy(table, f"acpi/{table.name}.dat")
    capture_laptop_state(output, errors)
    capture_device_details(output, errors)
    capture_machine_topology(output, errors)
    (output / "capture.json").write_text(json.dumps({
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "missing_or_partial": errors,
        "note": "No input events, serial numbers from DMI, network addresses or firmware settings were requested. USB descriptors may contain device identifiers."
    }, indent=2) + "\n")
    print(f"Captured Linux hardware evidence: {output.resolve()}")
    print(f"{len(errors)} unavailable/partial reads; see capture.json. ACPI usually requires sudo.")


if __name__ == "__main__":
    main()
