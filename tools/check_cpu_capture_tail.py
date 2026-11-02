#!/usr/bin/env python3
"""Check source-matched CPU/AP receipt retention, never physical acceptance.

Optional modeling only moves existing validated records to the new emission
points. It never changes timestamps, values, failure markers or strict parsers.
Modeled/padded text is synthetic evidence and cannot accept a native trial.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
from hardware_cpu_load import load_result
from hardware_ap_c3 import ap_c3_result
from hardware_package import package_result
from hardware_platform import platform_display_result, platform_series_result, platform_timing_result, platform_resources_result


def final_layout(text):
    lines = text.splitlines(keepends=True)
    banner = [x for x in lines if x.startswith('cpu-load: version=')]
    phases = [x for x in lines if re.match(r'cpu-load: phase=(idle|load|recovery) completed=yes ', x)]
    admission = [x for x in lines if x.startswith('ap-owner: v=1 admitted=1 ')]
    if len(banner) != 1 or len(phases) != 3 or len(admission) != 1 or sum(x == 'cpu-load: completed=yes\n' for x in lines) != 1:
        raise ValueError('need unique original header, ordered phases and admission')
    if [re.search(r'phase=(\w+)', x)[1] for x in phases] != ['idle', 'load', 'recovery']:
        raise ValueError('phase order')
    out = []
    for line in lines:
        if line in banner + phases:
            out.append(line.replace('cpu-load:', 'cpu-load-progress:', 1))
        elif line == admission[0]:
            out.append(line.replace('ap-owner:', 'ap-owner-progress:', 1))
        else:
            if line == 'cpu-load: completed=yes\n':
                out.extend(banner + phases)
            if line.startswith('ap-owner-cpu:') and not any(x == admission[0] for x in out):
                out.extend(admission)
            out.append(line)
    return ''.join(out)


def covered(text):
    p = load_result(text)
    display, series = platform_display_result(text), platform_series_result(text)
    return bool(p['completed'] and 'laptop-capture: completed=yes\n' in text
                and 'completed=NO' not in text
                and text.count('cpu-load: version=1 phases_seconds=30,30,30 workers=1 control_writes=none\n')==1
                and all(x['power'] and x['idle_residency'] and x['idle_entry'] for x in p['phases'])
                and p['idle_prerequisites'] and p['mwait'] and p['mwait']['status'] == 'PASS'
                and p['mwait']['hint']==16 and p['mwait']['residency']['valid']
                and ap_c3_result(text) and package_result(text)
                and display and not display['source_fault'] and display['source_completed']
                and series and series['continuity_ok']
                and ('ap-platform: v=5 ' not in text or
                     (platform_resources_result(text) and platform_resources_result(text)['status']=='OBSERVED')))


def critical_records(text):
    rows=[]
    for line in text.splitlines():
        if line.startswith('cpu-load:'):
            if ' completed=yes' in line or line.startswith(('cpu-load: version=', 'cpu-load: completed=')):
                rows.append(line)
        elif line.startswith(('cpu-load-capture:', 'cpu-load-power:', 'cpu-load-idle:',
                              'cpu-idle-', 'cpu-mwait', 'ap-owner:', 'ap-owner-cpu:',
                              'ap-c3:', 'ap-c3-peer:', 'ap-c3-round:', 'ap-package', 'ap-platform')):
            rows.append(line)
        elif line.startswith('laptop-capture: completed='):
            rows.append(line)
    return Counter(rows)


# Conservative capacities use existing source limits, not extra captured rows.
# Padding expands values without inventing semantic samples for the parsers.
CAPS = {'cpu-load-power: power_state=': 24, 'cpu-load-idle:': 24,
        'cpu-idle-entry:': 24, 'ap-platform-point:': 12,
        'ap-c3-round:': 32, 'ap-package-round:': 32,
        'ap-package-pci:': 50, 'ap-platform-ltr:': 25,
        'ap-platform-well:': 2, 'ap-platform-resources:': 2, 'ap-platform-resource:': 2}
WIDTHS = dict(version=1, v=1, phase=1, seq=2, us=5, id=10, seen=1,
              signature=6, ready_us=5, max_us=5, retire_us=5,
              state=1, fault=1, flags=4, energy_shift=2, nominal_mhz=4,
              active_mhz=20, package_mw=20, interval_us=7, rejected=1,
              energy=10, supported=4, valid=4, bdf=5, **{'class': 8},
              command=5, pm=5, link_cap=10, link=10, ltr_cap=10, ltr=5,
              ctl=10, before=10, after=10, index=2, reads=2, rc6=10, status=10, raw=10, headers=3, offset=4,
              cpus=1, bsp=1, mask=2, rounds=2, limit_us=5)


def width_padding(text):
    groups = {}
    for line in text.splitlines(keepends=True):
        if line.startswith('cpu-load: phase=') and ' completed=yes ' not in line:
            continue # Live progress precedes final proof, not retained proof.
        if not line.startswith(('cpu-load:', 'cpu-load-power:', 'cpu-load-idle:',
                                'cpu-idle-', 'cpu-mwait', 'ap-owner:', 'ap-owner-cpu:',
                                'ap-c3:', 'ap-c3-peer:', 'ap-c3-round:', 'ap-package', 'ap-platform')):
            continue
        prefix = next((key for key in CAPS if line.startswith(key)), line.split(':', 1)[0]+':')
        def expand(m):
            key, value = m.groups()
            width = 5 if re.fullmatch(r'c\d+_bp', key) else WIDTHS.get(key, 20)
            return key+'='+','.join('9'*max(width, len(v)) for v in value.split(','))
        expanded = re.sub(r'([a-zA-Z0-9_]+)=(\d+(?:,\d+)*)', expand, line)
        groups.setdefault(prefix, []).append((len(line.encode()), len(expanded.encode())))
    padding = 0
    for prefix, sizes in groups.items():
        capacity = max(len(sizes), CAPS.get(prefix, len(sizes)))
        padding += capacity*max(b for _, b in sizes)-sum(a for a, _ in sizes)
    return 'synthetic-receipt-width-and-capacity-padding:'+'x'*padding+'\n', padding


def assess(text, model=False):
    layout = final_layout(text) if model else text
    if not covered(layout):
        raise ValueError('full input lacks unchanged CPU/AP/package/display coverage')
    # Chatter stays intact. Additional pre-final chatter cannot displace a
    # proof which has not yet been emitted; exercise that explicitly too.
    pad, count = width_padding(layout)
    stressed = layout.replace('cpu-load: version=', 'synthetic-pre-final-chatter:'+'x'*131072+'\ncpu-load: version=', 1)
    tail = (stressed+pad).encode()[-65536:].decode(errors='replace')
    timing = platform_timing_result(layout)
    identical=critical_records(layout)==critical_records(tail)
    return dict(status='COVERAGE_PASS' if covered(tail) and identical else 'COVERAGE_FAIL',
                synthetic=True, physical_acceptance=False, model_final_layout=model,
                critical_records_preserved_byte_for_byte=identical,
                original_exit_tail_coverage=covered(text.encode()[-65536:].decode(errors='replace')),
                tail_bytes=65536, input_bytes=len(text.encode()),
                width_and_capacity_padding_bytes=count, pre_final_chatter_bytes=131072,
                original_rc6_bound_ok=timing['original_bound_ok'],
                input_sha256=hashlib.sha256(text.encode()).hexdigest())


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log', type=Path, required=True)
    ap.add_argument('--log-source-ref', required=True)
    ap.add_argument('--model-final-layout', action='store_true')
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    fingerprints = {}
    for name in ('kernel/src/workspace_log.c', 'kernel/src/scheduler.c'):
        old = subprocess.check_output(['git', 'show', f'{args.log_source_ref}:{name}'])
        current = Path(name).read_bytes()
        if old != current:
            ap.error('logging source differs from input trace: '+name)
        fingerprints[name] = hashlib.sha256(current).hexdigest()
    result = assess(args.log.read_text(), args.model_final_layout)
    result['logging_source_sha256'] = fingerprints
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))
    if result['status'] != 'COVERAGE_PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
