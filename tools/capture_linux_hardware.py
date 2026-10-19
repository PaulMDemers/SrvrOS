#!/usr/bin/env python3
"""Collect read-only PCI, USB, HID, ACPI, power and backlight evidence without input capture."""
import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess
import time


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


def capture_cpu_policy(output, errors, sys_root=Path("/sys")):
    """Read CPU policy attributes; never select a governor or enter an idle state.

    Enumerate first so a field disappearing during capture is a partial read,
    not silently reported as an unsupported attribute. Optional directories may
    be absent; inability to enumerate an existing directory is unknown instead.
    """
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    start_ns = time.monotonic_ns()
    root = sys_root / "devices/system/cpu"

    def names(path):
        try:
            return {entry.name for entry in path.iterdir()}
        except FileNotFoundError:
            return set()
        except OSError as error:
            errors.append(f"{path}: {error}")
            return None

    def read(path):
        try:
            return path.read_text().strip()
        except OSError as error:
            errors.append(f"{path}: {error}")
            return None

    def directory(path, fields, observed=False):
        # A discovered directory disappearing is also partial, even when empty.
        try:
            entries = {entry.name for entry in path.iterdir()}
        except FileNotFoundError as error:
            if observed:
                errors.append(f"{path}: {error}")
            return {"available": False, "properties": {}}, set()
        except OSError as error:
            errors.append(f"{path}: {error}")
            return {"available": None, "properties": {}}, set()
        return {"available": True, "properties": {
            name: read(path / name) for name in fields if name in entries
        }}, entries

    def numbered(entries, prefix):
        return sorted((name for name in entries
                       if re.fullmatch(prefix + r"[0-9]+", name)),
                      key=lambda name: int(name[len(prefix):]))

    result, entries = directory(root, ("online", "offline", "present", "possible", "kernel_max"))
    online_before = result["properties"].get("online")
    idle, _ = directory(root / "cpuidle", ("current_driver", "current_governor_ro",
        "current_governor", "available_governors"), "cpuidle" in entries)
    frequency, policies = directory(root / "cpufreq", ("boost",), "cpufreq" in entries)
    frequency["policies"] = []
    for name in numbered(policies, "policy"):
        item, _ = directory(root / "cpufreq" / name, ("affected_cpus", "related_cpus",
            "scaling_driver", "scaling_governor", "scaling_available_governors",
            "scaling_min_freq", "scaling_max_freq", "scaling_cur_freq",
            "scaling_available_frequencies", "cpuinfo_min_freq", "cpuinfo_max_freq",
            "cpuinfo_cur_freq", "cpuinfo_avg_freq", "cpuinfo_transition_latency",
            "energy_performance_preference", "energy_performance_available_preferences"), True)
        item["name"] = name
        frequency["policies"].append(item)
    cpus = []
    for name in numbered(entries, "cpu"):
        node = root / name
        item, cpu_entries = directory(node, ("online",), True)
        item["name"] = name
        item["topology"], _ = directory(node / "topology", ("physical_package_id",
            "die_id", "core_id", "core_cpus_list", "thread_siblings_list",
            "package_cpus_list", "core_siblings_list"), "topology" in cpu_entries)
        cpu_idle, states = directory(node / "cpuidle", (), "cpuidle" in cpu_entries)
        cpu_idle["states"] = []
        for state in numbered(states, "state"):
            value, _ = directory(node / "cpuidle" / state, ("name", "desc", "latency",
                "residency", "disable", "default_status", "usage", "time",
                "above", "below", "rejected"), True)
            value["index"] = int(state[5:])
            cpu_idle["states"].append(value)
        item["cpuidle"] = cpu_idle
        cpus.append(item)
    # Read online again even if the file vanished after its first observation.
    after_entries = names(root)
    online_after = (read(root / "online") if "online" in entries or
                    (after_entries is not None and "online" in after_entries) else None)
    result.update({"schema": "srvros-linux-cpu-policy-v1", "started_utc": started,
        "finished_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "duration_ns": time.monotonic_ns() - start_ns, "cpuidle": idle,
        "cpufreq": frequency, "cpus": cpus, "online_after": online_after,
        "online_changed": (online_before != online_after if online_before is not None
                           and online_after is not None else None),
        "units": {"frequency": "kHz", "cpuinfo_transition_latency": "ns",
                  "idle_latency_residency_time": "us", "idle_usage_above_below_rejected": "count"},
        "note": "Non-atomic read-only Linux policy snapshot, not energy or hardware residency measurement. "
                "Properties retain raw sysfs strings; absent optional attributes are omitted, failed reads "
                "are null and recorded in capture.json. Directory availability null means unknown. "
                "Matching online sets cannot exclude intervening hotplug or policy changes. "
                "No MSR reads, control writes or SrvrOS support are implied."})
    (output / "cpu-policy.json").write_text(json.dumps(result, indent=2) + "\n")


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
    capture_cpu_policy(output, errors)
    (output / "capture.json").write_text(json.dumps({
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "missing_or_partial": errors,
        "note": "No input events, serial numbers from DMI, network addresses or firmware settings were requested. USB descriptors may contain device identifiers."
    }, indent=2) + "\n")
    print(f"Captured Linux hardware evidence: {output.resolve()}")
    print(f"{len(errors)} unavailable/partial reads; see capture.json. ACPI usually requires sudo.")


if __name__ == "__main__":
    main()
