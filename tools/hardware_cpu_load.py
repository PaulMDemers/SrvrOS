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
                           power=phase_power(text,start,end)))
    return dict(completed=bool(valid),failure_reasons=failure,phases=phases if valid else [],
                scope='single-worker console workload; package power over covered subintervals, not whole-machine power')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exit_log', type=Path)
    args = p.parse_args()
    print(json.dumps(load_result(args.exit_log.read_text()),indent=2))
