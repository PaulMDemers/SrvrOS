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


def session_releases(text):
    """Use boot-option records, not commands or app text echoed into the log."""
    releases = set()
    for line in text.splitlines():
        record = next((p for p in ("cmdline: ", "desktop-trial: boot options=", "laptop-capture: boot options=") if line.startswith(p)), None)
        if record is None:
            continue
        for token in line[len(record):].split():
            prefix = "srvros.desktop-release="
            if token.startswith(prefix) and re.fullmatch(r"[0-9a-f]{16}", token[len(prefix):]):
                releases.add(token[len(prefix):])
    return sorted(releases)


def device_snapshot(line):
    if not line.startswith("workspace-devices: "):
        return None
    fields={}
    for token in line.split()[1:]:
        key,sep,value=token.partition("=")
        if not sep or not re.fullmatch(r"(?:-?[0-9]{1,20}|0x[0-9a-fA-F]{1,16})",value):
            return None
        fields[key]=int(value,16 if value.startswith("0x") else 10)
    for kind in ("audio","thermal","smc","pcm","power"):
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


def battery_diagnostic(exit_text):
    """Complete runtime evidence, never automatic physical-value acceptance."""
    lines=exit_text.splitlines()
    finish="battery-capture: returned=yes clean=yes; physical accuracy NOT_TESTED"
    if lines.count(finish)!=1 or any(line.startswith(("battery: HALTED", "battery: REFUSED")) for line in lines):return None
    complete=re.findall(r"^battery: PASS samples=5 notifications=([0-9]{1,20}) queries=([0-9]{1,20}) attached=([0-9]{1,20}) detached=([0-9]{1,20}) heap=0 errors=0 denied=0; physical accuracy NOT_TESTED$",exit_text,re.M)
    if len(complete)!=1 or complete[0][2]!=complete[0][3] or not int(complete[0][2]):return None
    pattern=(r"^battery: sample=(?P<generation>[0-9]{1,20}) ac_present=(?P<ac>[01]) soc=(?P<soc>[0-9]{1,3}) "
             r"voltage_uv=(?P<voltage_uv>[0-9]{1,20}) current_ua=(?P<current_ua>-?[0-9]{1,20}) "
             r"capacity=(?P<capacity>[0-9]{1,20}) full=(?P<full>[0-9]{1,20}) unit=(?P<unit>[12]) "
             r"temp_mc=(?P<temperature_mc>-?[0-9]{1,12}) valid=(?P<valid>0x[0-9a-f]{1,16}) age_us=(?P<age_us>[0-9]{1,20}) "
             r"ac_us=(?P<ac_us>[0-9]{1,20}) start_us=(?P<start_us>[0-9]{1,20}) end_us=(?P<end_us>[0-9]{1,20})$")
    samples=[]
    for match in re.finditer(pattern,exit_text,re.M):
        sample={k:int(v,16 if k=='valid' else 10) for k,v in match.groupdict().items()}
        if sample['generation']!=len(samples)+1 or sample['soc']>100 or sample['valid']&11!=11 or \
           not 0<=sample['ac_us']<=sample['start_us']<=sample['end_us']<2**64 or \
           sample['age_us']>1000000 or (samples and sample['ac_us']<=samples[-1]['end_us']):return None
        sample['capacity_unit']='uAh' if sample['unit']==1 else 'uWh'
        samples.append(sample)
    if len(samples)!=5 or sum(line.startswith('battery: sample=') for line in lines)!=5:return None
    return {'runtime_complete':True,'physical_accuracy':'manual-pending','samples':samples,
            'queries':int(complete[0][1]),'notifications':int(complete[0][0])}


def idle_sample(exit_text):
    records=re.findall(r"^idle-capture: elapsed_us=([0-9]{1,20}) sampled_ticks=([0-9]{1,20}) idle_ticks=([0-9]{1,20}) switches=([0-9]{1,20}) preemptions=([0-9]{1,20}) completed=yes$",exit_text,re.M)
    if len(records)!=1:return None
    elapsed,total,idle,switches,preemptions=map(int,records[0])
    if elapsed<59000000 or total==0 or idle>total:return None
    return {"elapsed_us":elapsed,"sampled_ticks":total,"idle_ticks":idle,
            "idle_sample_percent":round(100*idle/total,2),"switches":switches,
            "preemptions":preemptions,"scope":"console with periodic device and USB logging; not power or desktop acceptance"}


def desktop_idle_sample(exit_text):
    records=re.findall(r"^desktop-idle: elapsed_us=([0-9]{1,20}) sampled_ticks=([0-9]{1,20}) idle_ticks=([0-9]{1,20}) switches=([0-9]{1,20}) preemptions=([0-9]{1,20}) frames=([0-9]{1,20}) fb_bytes=([0-9]{1,20}) measured=yes$",exit_text,re.M)
    if len(records)!=1 or exit_text.splitlines().count("desktop-idle: completed=yes screenshot=yes")!=1 or \
       "desktop-idle-capture: returned=yes clean=yes" not in exit_text.splitlines() or "deskd: FAIL" in exit_text:return None
    elapsed,total,idle,switches,preemptions,frames,fb_bytes=map(int,records[0])
    if elapsed<59000000 or not total or idle>total:return None
    return {"elapsed_us":elapsed,"sampled_ticks":total,"idle_ticks":idle,
            "idle_sample_percent":round(100*idle/total,2),"switches":switches,
            "preemptions":preemptions,"frames":frames,"framebuffer_bytes":fb_bytes,
            "scope":"CPU0 scheduler samples with Text open and periodic device/USB logs; not energy or interactive acceptance"}


def power_idle_sample(exit_text):
    """Derive only the covered interval inside a completed desktop idle window.

    Sampling continues during screenshot writes. Those later measurements must
    not masquerade as idle energy. Missing/refused/invalid telemetry yields None.
    """
    idle=desktop_idle_sample(exit_text)
    window=re.findall(r"^desktop-idle-window: start_us=([0-9]{1,20}) end_us=([0-9]{1,20})$",exit_text,re.M)
    if not idle or len(window)!=1:return None
    start,end=map(int,window[0])
    if not 0<=start<end<2**64 or end-start!=idle["elapsed_us"]:return None
    samples={}
    required=("flags","energy_shift","nominal_mhz","sample_us","interval_us","samples","rejected","energy","aperf","mperf","monotonic_us")
    for line in exit_text.splitlines():
        parsed=device_snapshot(line)
        if not parsed or parsed[0]!="power":continue
        s=parsed[1]
        if any(k not in s for k in required):return None
        at=s["sample_us"]
        if at<start or s["monotonic_us"]>end:continue
        if s["power_state"]!=2 or s["flags"]!=3 or s["energy_shift"]!=14 or s["rejected"] or \
           not 0<s["nominal_mhz"]<=6400 or s["nominal_mhz"]%100 or not 100000<=s["interval_us"]<=5000000 or \
           not 0<=s["monotonic_us"]-at<=3000000 or not 0<=s["energy"]<2**32 or \
           any(not 0<=s[k]<2**64 for k in ("aperf","mperf","samples")):return None
        if at in samples and any(samples[at].get(k)!=v for k,v in s.items() if k!="monotonic_us"):return None
        samples[at]=s
    ordered=sorted(samples.values(),key=lambda s:s["sample_us"])
    if len(ordered)<3:return None
    first,last=ordered[0],ordered[-1];elapsed=last["sample_us"]-first["sample_us"]
    if elapsed<20000000:return None
    energy=0
    for a,b in zip(ordered,ordered[1:]):
        dt=b["sample_us"]-a["sample_us"]
        if not 0<dt<=20000000 or b["nominal_mhz"]!=first["nominal_mhz"] or \
           b["samples"]<=a["samples"] or b["aperf"]<a["aperf"] or b["mperf"]<a["mperf"]:return None
        uj=(((b["energy"]-a["energy"])&0xffffffff)*1000000)>>14
        if uj*1000//dt>100000:return None
        energy+=uj
    a=last["aperf"]-first["aperf"];m=last["mperf"]-first["mperf"]
    if not m:return None
    mhz=a*first["nominal_mhz"]//m
    if not 0<mhz<=6000:return None
    return {"start_us":first["sample_us"],"end_us":last["sample_us"],"covered_us":elapsed,
            "desktop_window_us":end-start,"checkpoints":len(ordered),"package_uj":energy,
            "average_package_mw":energy*1000//elapsed,"active_cpu0_mhz":mhz,
            "scope":"covered interval inside desktop idle; package energy and CPU0 active frequency, not whole-laptop power"}


def needs_attention(line):
    if any(token in line for token in ("SAVE FAILED","deskd: FAIL","reason=","exception:")):
        return True
    if line.startswith(("battery: HALTED", "battery: REFUSED", "battery-capture: returned=yes clean=NO")):return True
    if line.startswith("deskd: brightness requested=") and "result=-1" in line:
        return True
    if re.match(r"(?:audio|smc): state=4(?: |$)",line):
        return True
    parsed=device_snapshot(line)
    if not parsed:return False
    kind,fields=parsed
    if kind=="pcm":return fields["pcm_state"]==6 or fields.get("underruns",0)>0
    if kind=="power":return fields.get("rejected",0)>0 or (fields.get("flags",0)&4)!=0
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
        releases = session_releases(text)
        attention = sorted(set(line for line in text.splitlines() if needs_attention(line)))
        if len(releases) > 1:
            attention.append("collection: conflicting boot release identities")
        records.append({
            "session": name,
            "releases": releases,
            "files": {key: hashlib.sha256(data).hexdigest() for key, data in payload.items()},
            "exit_snapshot_present": "exit.log" in payload,
            "desktop_exit_seen": "deskd: exited" in text,
            "attention": attention,
            "devices": device_summaries(text),
            "desktop_idle_sample": desktop_idle_sample(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "power_idle_sample": power_idle_sample(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "battery_diagnostic": battery_diagnostic(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
            "idle_sample": idle_sample(payload.get("exit.log",b"").decode("utf-8",errors="replace")),
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
        release = ",".join(item["releases"]) or "unknown"
        print(f"  {item['session']}: release={release}, exit log={'yes' if item['exit_snapshot_present'] else 'missing'}, attention={len(item['attention'])}")
    print("Logs are diagnostic evidence, not an automatic hardware pass.")


if __name__ == "__main__":
    main()
