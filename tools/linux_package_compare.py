#!/usr/bin/env python3
"""Air6,1 host observations using kernel-mediated reads; never power policy.

PCI sysfs reads may cause Linux runtime PM to resume a device. Record the
surrounding power state; do not interpret a config-read D0 as its idle state.
"""
import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
from pathlib import Path
from hardware_package import package_result
from hardware_platform import platform_series_result


def decode(v):
    f = v['flags']
    return dict(bus_master_enabled=bool(v['command'] & 4),
                intx_disabled=bool(v['command'] & (1 << 10)),
                power_state=v['pm'] & 3 if f & 1 else None,
                pme_enabled=bool(v['pm'] & (1 << 8)) if f & 1 else None,
                aspm_enabled=v['link'] & 3 if f & 2 else None,
                aspm_supported=(v['link_cap'] >> 10) & 3 if f & 2 else None,
                common_clock=bool(v['link'] & (1 << 6)) if f & 2 else None,
                clock_pm_enabled=bool(v['link'] & (1 << 8)) if f & 2 else None,
                clock_pm_supported=bool(v['link_cap'] & (1 << 18)) if f & 2 else None,
                ltr_supported=bool(v['ltr_cap'] & (1 << 11)) if f & 4 else None,
                ltr_enabled=bool(v['ltr'] & (1 << 10)) if f & 4 else None)


def compare(native, linux):
    ns = {d['bdf']: d for d in native['devices']}
    ls = {}
    for row in linux:
        bdf = row['bdf']
        if bdf in ls or not 0 <= bdf < 65536:
            raise ValueError('duplicate/invalid PCI address')
        ls[bdf] = row
    result = []
    for bdf in sorted(ns.keys() | ls.keys()):
        n, l = ns.get(bdf), ls.get(bdf)
        if n is None or l is None:
            result.append(dict(bdf=bdf, status='missing-native' if n is None else 'missing-linux'))
            continue
        if l['state'] != 1 or l['read_error']:
            result.append(dict(bdf=bdf, status='unavailable-linux'))
            continue
        if (n['start']['id'], n['start']['class']) != (l['id'], l['class']):
            result.append(dict(bdf=bdf, status='identity-mismatch'))
            continue
        changes = {}
        for field in ('command', 'flags', 'pm', 'link_cap', 'link', 'ltr_cap', 'ltr'):
            # Keep start/end independently; native state need not be stable.
            a, b, c = n['start'][field], n['end'][field], l[field]
            if a != c or b != c:
                changes[field] = dict(native=[a, b], linux=c)
        result.append(dict(bdf=bdf, status='compared', changes=changes,
                           decoded_native=[decode(n['start']), decode(n['end'])],
                           decoded_linux=decode(l), linux_power_before=l.get('power_before'),
                           linux_power_after=l.get('power_after')))
    return dict(status='OBSERVED', acceptance=False, devices=result,
                scope='Different OS/workload snapshots; missing fields remain unknown; no causal blocker or policy-write authorization')


def text(path):
    try:
        return path.read_text().strip()
    except OSError as e:
        return dict(unavailable=e.strerror)


def power_state(path):
    return {n: text(path / n) for n in
            ('power_state', 'power/runtime_status', 'power/control')}


def msrs():
    rows = []
    for cpu in range(4):
        fd = os.open(f'/dev/cpu/{cpu}/msr', os.O_RDONLY)
        try:
            values = {}
            for offset in (0xe2, 0x60a, 0x1a0):
                raw = os.pread(fd, 8, offset)
                if len(raw) != 8:
                    raise ValueError('short MSR read')
                values[hex(offset)] = int.from_bytes(raw, 'little')
            rows.append(dict(cpu=cpu, values=values))
        finally:
            os.close(fd)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pci-reader', type=Path, required=True)
    ap.add_argument('--native-log', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    if args.output.exists():
        ap.error('output exists; preserve previous observations')
    if Path('/sys/class/dmi/id/product_name').read_text().strip() != 'MacBookAir6,1':
        ap.error('this captured MSR profile requires MacBookAir6,1')
    cpu = Path('/proc/cpuinfo').read_text().split('\n\n')[0]
    if not re.search(r'^vendor_id\s+: GenuineIntel$', cpu, re.M) or not re.search(r'^cpu family\s+: 6$', cpu, re.M) or not re.search(r'^model\s+: 69$', cpu, re.M):
        ap.error('refuse unsupported CPU MSR profile')
    root = Path('/sys/bus/pci/devices')
    for bdf, expected in [('0000:00:02.0', '0x0a26'), ('0000:00:1f.0', '0x9c43')]:
        if (root / bdf / 'vendor').read_text().strip() != '0x8086' or (root / bdf / 'device').read_text().strip() != expected:
            ap.error('refuse GPU/PCH identity mismatch')
    saved = args.native_log.read_text()
    native = package_result(saved)
    if native is None:
        ap.error('native log lacks complete package proof')
    args.output.mkdir(parents=True)
    metadata = dict(model='MacBookAir6,1', kernel=platform.release(),
                    native_exit_sha256=hashlib.sha256(args.native_log.read_bytes()).hexdigest(),
                    native_release=re.findall(r'srvros.desktop-release=([0-9a-f]{16})', saved),
                    sysfs_reads_may_resume_devices=True, userspace_control_writes=0,
                    msrs_before=msrs(), cpu_idle={})
    for c in range(4):
        metadata['cpu_idle'][c] = [dict(path=p.name, **{n: text(p / n) for n in
                                  ('name', 'desc', 'disable', 'latency', 'residency')})
                                  for p in sorted(Path(f'/sys/devices/system/cpu/cpu{c}/cpuidle').glob('state*'))]
    (args.output / 'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    devices = []
    for p in sorted(root.iterdir()):
        m = re.fullmatch(r'0000:([0-9a-f]{2}):([0-9a-f]{2})\.([0-7])', p.name)
        if not m:
            raise ValueError('unhandled PCI domain/address')
        if len(devices) >= 128:
            raise ValueError('PCI snapshot capacity exceeded')
        before = power_state(p)
        v = json.loads(subprocess.check_output([str(args.pci_reader.resolve()), str(p / 'config')], text=True))
        v.update(bdf=(int(m[1], 16) << 8) | (int(m[2], 16) << 3) | int(m[3]),
                 address=p.name, power_before=before, power_after=power_state(p),
                 driver=(p / 'driver').resolve().name if (p / 'driver').exists() else None)
        devices.append(v)
        (args.output / 'pci.json').write_text(json.dumps(devices, indent=2)+'\n')
    metadata['msrs_after'] = msrs()
    metadata['msrs_unchanged'] = metadata['msrs_before'] == metadata['msrs_after']
    (args.output / 'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    result = compare(native, devices)
    (args.output / 'comparison.json').write_text(json.dumps(result, indent=2)+'\n')
    (args.output / 'native-series.json').write_text(json.dumps(platform_series_result(saved), indent=2)+'\n')
    print('Observed', len(devices), 'PCI functions; comparison saved, no causal inference.')


if __name__ == '__main__':
    main()
