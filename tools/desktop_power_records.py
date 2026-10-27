"""Strict automatic desktop power phases from bounded saved checkpoints."""
import re
from hardware_cpu_load import phase_power
from hardware_desktop import device_snapshot
from scheduler_activity import worker_activity


def merged_records(logs):
    """Deduplicate overlapping ring snapshots; never repair incomplete records."""
    blocks, devices = {}, {}
    for text in logs.values():
        if isinstance(text, bytes):
            text = text.decode('utf-8', errors='strict')
        active = None
        for line in text.splitlines():
            if line.startswith('worker-snapshot:'):
                assert active is None, 'nested worker snapshot'
                active = [line]
            elif line.startswith(('worker:', 'worker-snapshot-end:')):
                if active is None:
                    continue  # Ring tail can begin within a prior block.
                active.append(line)
                if line.startswith('worker-snapshot-end:'):
                    at = int(re.fullmatch(r'worker-snapshot: us=(\d+) count=\d+ version=[12]', active[0])[1])
                    block = '\n'.join(active)
                    assert at not in blocks or blocks[at] == block, 'conflicting worker snapshot'
                    blocks[at] = block
                    active = None
            elif line.startswith('workspace-devices:'):
                parsed = device_snapshot(line)
                assert parsed, 'malformed device checkpoint'
                kind, fields = parsed
                key = (kind, fields['monotonic_us'])
                assert key not in devices or devices[key] == line, 'conflicting device checkpoint'
                devices[key] = line
        assert active is None, 'incomplete worker snapshot at end'
    return '\n'.join([*(blocks[k] for k in sorted(blocks)), *(devices[k] for k in sorted(devices))])


def desktop_power_result(logs, native=True):
    assert 'exit.log' in logs, 'missing final log'
    final = logs['exit.log']
    if isinstance(final, bytes): final = final.decode('utf-8', errors='strict')
    assert re.search(r'^battery-service: retired epoch=1 .*heap=0 errors=0 denied=0$', final, re.M)
    assert re.search(r'^battery-controls: PASS .*revoked=yes retired=yes$', final, re.M)
    receipt = re.findall(r'^desktop-power-session: result=PASS cycles=(\d+) errors=0 normal_exit=1 max_loop_us=(\d+) max_ready_us=(\d+) max_close_us=(\d+)$', final, re.M)
    assert len(receipt) == 1, 'missing unique completion'
    cycles, loop, ready, close = map(int, receipt[0])
    assert cycles >= 6 and 0 < loop <= 500000 and 0 < ready <= 10000000 and 0 < close <= 10000000
    rows = re.findall(r'^desktop-power-phase: name=(idle|apps|recovery) start_us=(\d+) end_us=(\d+) battery_first=(\d+) battery_last=(\d+)$', final, re.M)
    assert [r[0] for r in rows] == ['idle', 'apps', 'recovery'], 'missing ordered phases'
    text = merged_records(logs)
    phases, last = [], None
    for name, start, end, first, generation in rows:
        start, end, first, generation = map(int, (start, end, first, generation))
        assert 0 < start < end < 2**63 and 60000000 <= end-start <= 65000000
        assert last is None or last == start
        assert 0 < first < generation < 2**64
        last = end
        workers = worker_activity(text, start, end)
        assert workers and workers['workers'], f'{name}: missing worker coverage'
        power = phase_power(text, start, end)
        thermal = []
        for line in text.splitlines():
            parsed = device_snapshot(line)
            if parsed and parsed[0] == 'thermal':
                s = parsed[1]
                if start <= s.get('sample_us', -1) <= s['monotonic_us'] <= end and s['thermal_state'] == 2:
                    assert s['flags'] & 3 == 3 and not s['flags'] & 12
                    assert s['monotonic_us']-s['sample_us'] <= 3000000
                    assert all(0 <= s[k] < 90000 for k in ('core_mc', 'package_mc'))
                    thermal.append(s)
        if native:
            assert power and power['covered_us'] >= 20000000 and power['checkpoints'] >= 3, f'{name}: missing power coverage'
            assert len(thermal) >= 3 and max(s['sample_us'] for s in thermal)-min(s['sample_us'] for s in thermal) >= 20000000, f'{name}: missing thermal coverage'
        else:
            assert power is None and not thermal, 'VM must not fabricate native telemetry'
        temperatures = {key: dict(min=min(s[key] for s in thermal), max=max(s[key] for s in thermal)) for key in ('core_mc','package_mc')} if thermal else None
        phases.append(dict(name=name, start_us=start, end_us=end, battery_first=first, battery_last=generation,
                           power=power, temperatures=temperatures, worker_activity=workers))
    return dict(status='PASS', native_measurements=native, cycles=cycles, max_loop_us=loop, max_ready_us=ready,
                max_close_us=close, phases=phases, scope='Covered CPU package energy, CPU0 active frequency, core/package temperatures and software worker wakeups; no power policy changes')
