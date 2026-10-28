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


def phase_idle_entry(text, start, end):
    """HLT attempts/returns are scheduler observations, not residency durations."""
    headers=[s for s in text.splitlines() if s.startswith('cpu-idle-audit:')]
    entries=[s for s in text.splitlines() if s.startswith('cpu-idle-entry:')]
    idle=[s for s in text.splitlines() if s.startswith('cpu-load-idle:')]
    if len(headers)!=1 or len(entries)!=len(idle) or phase_idle(text,start,end) is None: return None
    def fields(line,keys):
        parts=line.split()[1:]
        if any(not re.fullmatch(r'[a-z0-9_]+=[a-z0-9-]+',p) for p in parts): return None
        d=dict(p.split('=') for p in parts)
        if len(d)!=len(parts) or set(d)!=keys: return None
        for k in keys-{'entry'}:
            if not d[k].isdigit() or int(d[k])>=2**64: return None
            d[k]=int(d[k])
        return d
    h=fields(headers[0],set('version entry policy_writes managed_cpus other_cpus_known monitor leaf1_b leaf5_a leaf5_b leaf5_c leaf5_d e2_start e2_end epoch enters returns ready'.split()))
    if not h or h['version']!=1 or h['entry']!='sti-hlt-cli' or h['policy_writes'] or h['managed_cpus']!=1 or h['other_cpus_known'] or h['monitor'] not in (0,1) or not h['epoch']: return None
    if any(h[k]>=2**32 for k in ('leaf1_b','leaf5_a','leaf5_b','leaf5_c','leaf5_d')): return None
    if not h['monitor'] and any(h[k] for k in ('leaf5_a','leaf5_b','leaf5_c','leaf5_d')): return None
    rows=[]
    for line,paired in zip(entries,idle):
        d=fields(line,{'sample_us','epoch','enters','returns','ready'})
        stamps=re.findall(r' sample_us=(\d+)(?: |$)',paired)
        if not d or d['epoch']!=h['epoch'] or len(stamps)!=1 or d['sample_us']!=int(stamps[0]): return None
        if rows and d['sample_us']<=rows[-1]['sample_us']: return None
        rows.append(d)
    for d in rows+[h]:
        if not 0<=d['enters']-d['returns']<=1: return None
    for a,b in zip(rows,rows[1:]+[h]):
        if any(b[k]<a[k] for k in ('enters','returns','ready')): return None
    selected=[d for d in rows if start<=d['sample_us']<=end]
    if len(selected)<2 or selected[-1]['sample_us']-selected[0]['sample_us']<10000000: return None
    a,b=selected[0],selected[-1]
    def config(raw):
        limit=raw&15
        names=('C0/C1','C2','C3','C6','C7','C7s','C8','C9','C10')
        return dict(raw=raw,limit_encoding=limit,limit=names[limit] if limit<len(names) else 'unknown',
                    cfg_lock=(raw>>15)&1,io_mwait_redirection=(raw>>10)&1,
                    c3_demotion=(raw>>25)&1,c1_demotion=(raw>>26)&1,
                    c3_undemotion=(raw>>27)&1,c1_undemotion=(raw>>28)&1)
    return dict(covered_us=b['sample_us']-a['sample_us'],checkpoints=len(selected),epoch=h['epoch'],
                enters=b['enters']-a['enters'],returns=b['returns']-a['returns'],ready_bypasses=b['ready']-a['ready'],
                firmware_start=config(h['e2_start']),firmware_end=config(h['e2_end']),
                firmware_changed=h['e2_start']!=h['e2_end'],monitor=h['monitor'],
                cpuid5=[h[k] for k in ('leaf5_a','leaf5_b','leaf5_c','leaf5_d')],
                cpuid_logical_capacity=(h['leaf1_b']>>16)&255,initial_apic_id=h['leaf1_b']>>24,
                other_cpus_state='unknown',scope='CPU0 HLT attempts/returns; no time-asleep or C-state inference')

def idle_prerequisites(text):
    """CPU0 capability/configuration only; no observation of live remote CPUs.

    Keep unknown topology layouts explicit rather than guessing a CPU count.
    Historical captures without this new record remain readable, but cannot
    satisfy the new native prerequisite trial's receipt requirement.
    """
    lines=[s for s in text.splitlines() if s.startswith('cpu-idle-prereq:')]
    if len(lines)!=1: return None
    parts=lines[0].split()[1:]
    if any(not re.fullmatch(r'[a-z0-9_]+=\d+(?:,\d+)*',p) for p in parts): return None
    row=dict(p.split('=') for p in parts)
    if len(row)!=len(parts) or set(row)!={'v','epoch','max','b0','b1','b2','apic','misc'}: return None
    widths={'v':1,'epoch':1,'max':1,'b0':4,'b1':4,'b2':4,'apic':2,'misc':2}
    for key,value in row.items():
        values=[int(v) for v in value.split(',')]
        bound=2**64 if key in ('epoch','apic','misc') else 2**32
        if len(values)!=widths[key] or any(v>=bound for v in values): return None
        row[key]=values if widths[key]>1 else values[0]
    if row['v']!=1 or not row['epoch']: return None
    # Bind it to the complete audit/capture, not a stray boot/status line.
    phases=re.findall(r'^cpu-load: phase=idle completed=yes elapsed_us=\d+ start_us=(\d+) end_us=(\d+) chunks=0 checksum=\d+$',text,re.M)
    if len(phases)!=1: return None
    audit=phase_idle_entry(text,*map(int,phases[0]))
    if not audit or audit['epoch']!=row['epoch']: return None
    raw=[row[f'b{i}'] for i in range(3)];topology=None
    if row['max']<11:
        if any(v for level in raw for v in level): return None
    else:
        a,b,z=raw
        # Captured Haswell SMT/core/terminator layout only. Counts describe
        # addressable topology, not enabled CPUs or remote execution state.
        if (a[2]==256 and b[2]==513 and z[:3]==[0,0,2] and
            a[3]==b[3]==z[3] and 0<=a[0]<=b[0]<=31 and
            0<a[1]<=b[1]<=65535 and b[1]%a[1]==0 and
            a[1]<=2**a[0] and b[1]<=2**b[0]):
            topology=dict(x2apic_id=a[3],smt_shift=a[0],package_shift=b[0],
                          logical_per_core=a[1],logical_per_package=b[1],
                          cores_per_package=b[1]//a[1],scope='CPUID addressable topology, not online CPU count')
    def config(apic,misc):
        return dict(apic_raw=apic,misc_raw=misc,bsp=(apic>>8)&1,
                    apic_enabled=(apic>>11)&1,x2apic_enabled=(apic>>10)&1,
                    monitor_fsm=(misc>>18)&1)
    start=config(row['apic'][0],row['misc'][0]);end=config(row['apic'][1],row['misc'][1])
    return dict(epoch=row['epoch'],max_basic_leaf=row['max'],raw_leaf_b=raw,topology=topology,
                config_start=start,config_end=end,configuration_changed=start!=end,
                monitor_advertised=bool(audit['monitor']),
                monitor_line_min=audit['cpuid5'][0],monitor_line_max=audit['cpuid5'][1],
                monitor_fsm_consistent=bool(start['monitor_fsm']==end['monitor_fsm']==audit['monitor']),
                mwait_extensions=bool(audit['cpuid5'][2]&1),
                masked_interrupt_break=bool(audit['cpuid5'][2]&2),
                cpuid5_substate_nibbles=[(audit['cpuid5'][3]>>(4*i))&15 for i in range(8)],
                other_cpus_state='unknown',remote_observation=False,
                scope='CPU0 read-only prerequisites; no AP startup/IPI/MWAIT or power-policy changes')


def mwait_result(text):
    lines=[s for s in text.splitlines() if s.startswith('cpu-mwait:')]
    if len(lines)!=1: return None
    if lines[0] in ('cpu-mwait: v=1 admitted=0 fallback=hlt','cpu-mwait: v=2 admitted=0 fallback=hlt'):
        return dict(status='refused',fallback='hlt')
    parts=lines[0].split()[1:]
    keys=set('v admitted epoch hint ext cap limit_us tries returns max_us fault hlt rechecks tick_begin tick_end'.split())
    if any(not re.fullmatch(r'[a-z_]+=\d+',s) for s in parts): return None
    row={k:int(v) for k,v in (s.split('=') for s in parts)}
    if len(row)!=len(parts) or set(row)!=keys or any(v>=2**64 for v in row.values()): return None
    if ((row['v'],row['hint']) not in ((1,0),(2,16)) or row['admitted']!=1 or row['ext']!=1 or
        row['cap']!=32 or row['limit_us']!=20000 or row['tries']>32 or row['returns']!=row['tries'] or
        row['hlt']>32 or row['fault'] not in ((0,1,2,3) if row['v']==2 else (0,1,2))): return None
    prereq=idle_prerequisites(text)
    if not prereq or prereq['epoch']!=row['epoch'] or prereq['configuration_changed']: return None
    c=prereq['config_start']
    if (not prereq['monitor_fsm_consistent'] or not prereq['monitor_advertised'] or
        not prereq['mwait_extensions'] or not prereq['masked_interrupt_break'] or
        not prereq['cpuid5_substate_nibbles'][1] or prereq['monitor_line_min']!=64 or prereq['monitor_line_max']!=64 or
        c['bsp']!=1 or c['apic_enabled']!=1 or c['x2apic_enabled']!=0): return None
    audit_line=next(s for s in text.splitlines() if s.startswith('cpu-idle-audit:'))
    counts=dict(s.split('=') for s in audit_line.split()[1:])
    if int(counts['enters'])<row['hlt'] or int(counts['returns'])+1<row['hlt']: return None
    deep=None
    deep_lines=[s for s in text.splitlines() if s.startswith('cpu-mwait-deep:')]
    if row['v']==2:
        if len(deep_lines)!=1:return None
        fields=deep_lines[0].split()[1:]
        if any(not re.fullmatch(r'[a-z_0-9]+=\d+',s) for s in fields):return None
        deep={k:int(v) for k,v in (s.split('=') for s in fields)}
        if (len(fields)!=len(deep) or set(deep)!=set('v epoch arat timer tsc core_c3 package_c3'.split()) or
            any(v>=2**64 for v in deep.values()) or deep['v']!=1 or deep['epoch']!=row['epoch'] or
            deep['arat']!=1 or deep['timer'] not in (0,1) or
            deep['core_c3']>deep['tsc'] or deep['package_c3']>deep['tsc'] or
            not prereq['cpuid5_substate_nibbles'][2] or
            int(counts['e2_start'])!=int(counts['e2_end']) or not 2<=int(counts['e2_start'])&15<=8):return None
    elif deep_lines:return None
    # A fallback is a safety outcome, not successful MWAIT acceptance. Require
    # real tick progress during attempts and the mandatory subsequent HLT path.
    passed=(row['tries']==32 and row['hlt']==32 and not row['fault'] and
            0<row['max_us']<=20000 and row['tick_end']>row['tick_begin'] and
            (deep is None or (deep['timer']==1 and deep['tsc']>0)))
    result=dict(status='PASS' if passed else 'FAILED_OR_INCOMPLETE',**row,
                scope='32 bounded CPU0 waits; masked wait duration, not IRQ dispatch latency')
    if deep is not None:
        deep['valid']=passed
        deep['core_c3_observed']=bool(passed and deep['core_c3'])
        deep['package_c3_observed']=bool(passed and deep['package_c3'])
        result['residency']=deep
    return result


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
                           power=phase_power(text,start,end),idle_residency=phase_idle(text,start,end),idle_entry=phase_idle_entry(text,start,end)))
    return dict(completed=bool(valid),failure_reasons=failure,phases=phases if valid else [],
                idle_prerequisites=idle_prerequisites(text) if valid else None,mwait=mwait_result(text),
                scope='single-worker console workload; package power over covered subintervals, not whole-machine power')

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exit_log', type=Path)
    args = p.parse_args()
    print(json.dumps(load_result(args.exit_log.read_text()),indent=2))
