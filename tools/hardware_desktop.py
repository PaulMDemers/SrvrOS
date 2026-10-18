#!/usr/bin/env python3
"""Collect preserved native-desktop USB diagnostics after returning to Ubuntu."""
import argparse
import hashlib
import json
import re
import tempfile
from pathlib import Path
import hardware_test as host

SESSION = re.compile(r"session-[0-9a-f]{16}-[0-9a-f]{2}")
FILES = ("boot.log", "session-a.log", "session-b.log", "exit.log")


def device_snapshot(line):
    if not line.startswith("workspace-devices: "):
        return None
    fields={}
    for token in line.split()[1:]:
        key,sep,value=token.partition("=")
        if not sep or not re.fullmatch(r"(?:-?[0-9]{1,20}|0x[0-9a-fA-F]{1,16})",value):
            return None
        fields[key]=int(value,16 if value.startswith("0x") else 10)
    for kind in ("audio","thermal","smc"):
        if kind+"_state" in fields and fields.get("monotonic_us",-1)>=0:
            return kind,fields
    return None


def device_summaries(text):
    latest={}
    for line in text.splitlines():
        parsed=device_snapshot(line)
        if parsed:
            kind,fields=parsed
            if kind not in latest or fields["monotonic_us"]>=latest[kind]["monotonic_us"]:
                latest[kind]=fields
    return latest


def needs_attention(line):
    if any(token in line for token in ("SAVE FAILED","deskd: FAIL","reason=","exception:")):
        return True
    if line.startswith("deskd: brightness requested=") and "result=-1" in line:
        return True
    if re.match(r"(?:audio|smc): state=4(?: |$)",line):
        return True
    parsed=device_snapshot(line)
    if not parsed:return False
    kind,fields=parsed
    return (kind in ("audio","smc") and fields[kind+"_state"]==4) or (kind=="thermal" and fields.get("flags",0)&8)!=0


def collect(mount, volume, output):
    workspace = mount / "srvros-work"
    marker = workspace / ".workspace"
    if workspace.is_symlink() or marker.is_symlink() or not marker.is_file():
        raise ValueError("Prepared workspace missing or unsafe")
    expected = f"SRVROS-WORKSPACE-V1 {volume}\n".encode()
    if marker.stat().st_size != len(expected) or marker.read_bytes() != expected:
        raise ValueError("Workspace marker does not match the USB volume")
    logs = workspace / "Logs"
    if logs.is_symlink() or not logs.is_dir():
        raise ValueError("No desktop session logs on this volume yet")
    sessions = sorted(p for p in logs.iterdir() if SESSION.fullmatch(p.name))
    if not sessions or len(sessions) > 4096:
        raise ValueError("No sessions found, or session count exceeds collection limit")
    # Validate and read everything before creating the local collection.
    snapshots = []
    total_bytes = 0
    for folder in sessions:
        if folder.is_symlink() or not folder.is_dir():
            raise ValueError(f"Unsafe session path: {folder.name}")
        payload = {}
        for name in FILES:
            source = folder / name
            if source.is_symlink():
                raise ValueError(f"Unsafe log path: {folder.name}/{name}")
            if not source.exists():
                continue
            if not source.is_file() or source.stat().st_size > 65536:
                raise ValueError(f"Invalid log: {folder.name}/{name}")
            total_bytes += source.stat().st_size
            if total_bytes > 64*1024*1024:
                raise ValueError("Collection exceeds 64 MiB; archive older session folders first")
            payload[name] = source.read_bytes()
            if len(payload[name]) > 65536:
                raise ValueError("Log changed while reading")
        snapshots.append((folder.name, payload))
    output.mkdir(parents=True, exist_ok=True)
    destination = Path(tempfile.mkdtemp(prefix="collection-", dir=output))
    records = []
    for name, payload in snapshots:
        saved = destination / name
        saved.mkdir()
        for filename, data in payload.items():
            (saved / filename).write_bytes(data)
            if (saved / filename).read_bytes() != data:
                raise OSError("Local collection verification failed")
        text = "\n".join(data.decode("utf-8", errors="replace") for data in payload.values())
        records.append({
            "session": name,
            "files": {key: hashlib.sha256(data).hexdigest() for key, data in payload.items()},
            "exit_snapshot_present": "exit.log" in payload,
            "desktop_exit_seen": "deskd: exited" in text,
            "attention": sorted(set(line for line in text.splitlines() if needs_attention(line))),
            "devices": device_summaries(text),
            "probe": sorted(set(line for line in text.splitlines() if line.startswith("laptop-probe:"))),
            "brightness": sorted(set(line for line in text.splitlines() if line.startswith(("backlight:", "deskd: brightness", "desktools: brightness")))),
        })
    report = {"format": "srvros-desktop-collection-v1", "volume": volume,
              "hardware_acceptance": "manual-pending", "sessions": records}
    (destination / "collection.json").write_text(json.dumps(report, indent=2) + "\n")
    return destination, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uuid", default="37d8c5d0")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "build/desktop-collections")
    args = parser.parse_args()
    try:
        volume = host.volume_id(args.uuid)
        destination, report = collect(host.volume_mount(volume), volume, args.output)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"desktop-collect: {error}\n")
    print(f"desktop-collect: {len(report['sessions'])} sessions copied and verified: {destination}")
    for item in report["sessions"]:
        print(f"  {item['session']}: exit log={'yes' if item['exit_snapshot_present'] else 'missing'}, attention={len(item['attention'])}")
    print("Logs are diagnostic evidence, not an automatic hardware pass.")


if __name__ == "__main__":
    main()
