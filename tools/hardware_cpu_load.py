#!/usr/bin/env python3
"""Parse only a complete, distinct CPU-load capture; never treat it as idle."""
import argparse
import json
from pathlib import Path
import re
from hardware_desktop import device_snapshot


def phase_power(text, start, end):
    samples = {}
    # Deferred records retain their sampling timestamp, not their later print
    # time. Never accept a truncated/overflowed bounded capture as coverage.
    captured = [line for line in text.splitlines() if line.startswith('cpu-load-power: power_state=')]
    receipts = re.findall(r'^cpu-load-power: retained=(\d+) overflow=(\d+)$', text, re.M)
    if captured or receipts:
        if len(receipts) != 1 or receipts[0][1] != '0' or not 0 < int(receipts[0][0]) == len(captured) <= 24:
            return None
    required = ('flags','energy_shift','nominal_mhz','sample_us','interval_us',
                'samples','rejected','energy','aperf','mperf','monotonic_us')
    for line in text.splitlines():
        retained = line.startswith('cpu-load-power: power_state=')
        if retained:
            line = line.replace('cpu-load-power:', 'workspace-devices:', 1)
        parsed = device_snapshot(line)
        if retained and (not parsed or parsed[0] != 'power'):
            return None
        if not parsed or parsed[0] != 'power':
            continue
        s = parsed[1]
        if any(k not in s for k in required):
            return None
        at = s['sample_us']
        if at < start or s['monotonic_us'] > end:
            continue
        if (s['power_state'] != 2 or s['flags'] != 3 or s['energy_shift'] != 14 or
            s['rejected'] or not 0 < s['nominal_mhz'] <= 6400 or s['nominal_mhz'] % 100 or
            not 100000 <= s['interval_us'] <= 5000000 or
            not 0 <= s['monotonic_us']-at <= 3000000 or not 0 <= s['energy'] < 2**32 or
            any(not 0 <= s[k] < 2**64 for k in ('aperf','mperf','samples'))):
            return None
        if at in samples and any(samples[at].get(k) != v for k,v in s.items() if k != 'monotonic_us'):
            return None
        samples[at] = s
    ordered = sorted(samples.values(), key=lambda s:s['sample_us'])
    if len(ordered) < 2:
        return None
    first,last = ordered[0],ordered[-1]
    elapsed = last['sample_us']-first['sample_us']
    if elapsed < 10000000:
        return None
    energy = 0
    for a,b in zip(ordered,ordered[1:]):
        dt = b['sample_us']-a['sample_us']
        if (not 0 < dt <= 20000000 or b['nominal_mhz'] != first['nominal_mhz'] or
            b['samples'] <= a['samples'] or b['aperf'] < a['aperf'] or b['mperf'] < a['mperf']):
            return None
        uj = (((b['energy']-a['energy']) & 0xffffffff)*1000000) >> 14
        if uj*1000//dt > 100000:
            return None
        energy += uj
    m = last['mperf']-first['mperf']
    if not m:
        return None
    mhz = (last['aperf']-first['aperf'])*first['nominal_mhz']//m
    if not 0 < mhz <= 6000:
        return None
    return dict(start_us=first['sample_us'],end_us=last['sample_us'],covered_us=elapsed,
                checkpoints=len(ordered),package_uj=energy,average_package_mw=energy*1000//elapsed,
                active_cpu0_mhz=mhz)



IDLE_NAMES = ('core_c3','core_c6','core_c7','package_c2','package_c3',
              'package_c6','package_c7','package_c8','package_c9','package_c10')

def phase_idle(text, start, end):
    """Read-only 06_45 counters, separate scopes and conservative TSC brackets.

    Exact retained receipt/power pairing is required. Zero is a measurement;
    missing, stale, reset, truncated or malformed telemetry is unavailable.
    """
    lines = [line for line in text.splitlines() if line.startswith('cpu-load-idle:')]
    receipts = re.findall(r'^cpu-load-power: retained=(\d+) overflow=(\d+)$', text, re.M)
    power_lines = [line for line in text.splitlines() if line.startswith('cpu-load-power: power_state=')]
    if len(receipts)!=1 or receipts[0][1]!='0' or not 0<len(lines)==len(power_lines)==int(receipts[0][0])<=24:
        return None
    keys = {'supported','valid','rejected','sample_us','interval_us','interval_tsc','tsc_begin','tsc_end'}
    keys.update(f'c{i}_{suffix}' for i in range(10) for suffix in ('raw','bp'))
    samples = []
    for line,power in zip(lines,power_lines):
        fields=line.split()[1:]
        if any(not re.fullmatch(r'[a-z0-9_]+=\d+',f) for f in fields): return None
        row={k:int(v) for k,v in (f.split('=') for f in fields)}
        if set(row)!=keys or len(fields)!=len(keys) or any(v>=2**64 for v in row.values()): return None
        stamps=re.findall(r' sample_us=(\d+)(?: |$)',power)
        if len(stamps)!=1 or row['sample_us']!=int(stamps[0]): return None
        if (row['supported']!=1023 or row['valid']!=1023 or row['rejected'] or
            not 100000<=row['interval_us']<=5000000 or not row['interval_tsc'] or
            not 0<row['tsc_begin']<=row['tsc_end'] or
            row['tsc_end']-row['tsc_begin']>row['interval_tsc'] or
            any(row[f'c{i}_bp']>10000 for i in range(10)) or
            any(row[f'c{i}_raw']>=2**60 for i in range(7,10)) or
            sum(row[f'c{i}_bp'] for i in range(3))>10000 or
            sum(row[f'c{i}_bp'] for i in range(3,10))>10000): return None
        if samples and (row['sample_us']<=samples[-1]['sample_us'] or row['tsc_begin']<=samples[-1]['tsc_end']): return None
        samples.append(row)
    selected=[s for s in samples if start<=s['sample_us']<=end]
    if len(selected)<2 or selected[-1]['sample_us']-selected[0]['sample_us']<10000000: return None
    for a,b in zip(samples,samples[1:]):
        if b['sample_us']-a['sample_us']>20000000: return None
        dt=b['tsc_end']-a['tsc_begin']
        delta=[b[f'c{i}_raw']-a[f'c{i}_raw'] for i in range(10)]
        if any(n<0 or n>dt for n in delta) or sum(delta[:3])>dt or sum(delta[3:])>dt: return None
    a,b=selected[0],selected[-1];dt=b['tsc_end']-a['tsc_begin']
    return dict(covered_us=b['sample_us']-a['sample_us'],checkpoints=len(selected),
                start_us=a['sample_us'],end_us=b['sample_us'],tsc_ticks=dt,
                basis_points={name:(b[f'c{i}_raw']-a[f'c{i}_raw'])*10000//dt for i,name in enumerate(IDLE_NAMES)},
                scope='CPU0 physical core and whole package; lower-bound TSC-bracket ratios; no inferred C0/C1')

def load_result(text):
    failure = re.findall(r'^cpu-load: completed=NO reason=(\S+)$',text,re.M)
    rows = re.findall(r'^cpu-load: phase=(idle|load|recovery) completed=yes elapsed_us=(\d+) start_us=(\d+) end_us=(\d+) chunks=(\d+) checksum=(\d+)$',text,re.M)
    valid = (not failure and text.count('cpu-load: completed=yes\n') == 1 and
             'cpu-load-capture: returned=yes process_ok=yes' in text and [r[0] for r in rows] == ['idle','load','recovery'])
    phases = []
    last = None
    for name,elapsed,start,end,chunks,_ in rows:
        elapsed,start,end,chunks = map(int,(elapsed,start,end,chunks))
        if not (0 <= start < end < 2**63 and end-start == elapsed and 30000000 <= elapsed <= 35000000):
            valid = False
        if last is not None and start != last:
            valid = False
        if (name == 'load' and not chunks) or (name != 'load' and chunks):
            valid = False
        last = end
        phases.append(dict(name=name,start_us=start,end_us=end,elapsed_us=elapsed,chunks=chunks,
                           power=phase_power(text,start,end),idle_residency=phase_idle(text,start,end)))
    return dict(completed=bool(valid),failure_reasons=failure,phases=phases if valid else [],
                scope='single-worker console workload; package power over covered subintervals, not whole-machine power')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exit_log', type=Path)
    args = p.parse_args()
    print(json.dumps(load_result(args.exit_log.read_text()),indent=2))
