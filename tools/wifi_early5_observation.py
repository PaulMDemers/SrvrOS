"""Strict bounded early-RX observations, without protocol/ownership acceptance."""
import re
from wifi_mcu_loss_trace import observe as observe_loss

SUMMARY = ('version complete posted completed exhausted during_mcu during_control '
           'idle_events fault calibration commands operations mac stopped frozen retained reused '
           'inspected network_out').split()
BOUNDARY = 'version step completed_before completed_after'.split()


def require(ok, why):
    if not ok:
        raise ValueError(why)


def row(line, prefix, keys):
    pattern = prefix + ': ' + ' '.join(k + (r'=(0x[0-9a-f]{16})' if k == 'ring' else r'=(0|[1-9][0-9]*)') for k in keys)
    m = re.fullmatch(pattern, line)
    require(m is not None and len(line) < 1024, 'invalid early5 row/schema')
    d = {k: int(v, 16 if k == 'ring' else 10) for k, v in zip(keys, m.groups())}
    require(all(v < 2**(64 if k == 'ring' else 32) for k, v in d.items()) and d['version'] == 1,
            'invalid early5 version/range')
    return d


def observe(log):
    lines = [l for l in log.splitlines() if l.lstrip().startswith('wifi-early5')]
    summaries = [l for l in lines if l.startswith('wifi-early5:')]
    require(len(summaries) <= 1, 'duplicate early5 summary')
    result = dict(present=bool(summaries), native_accepted=False,
                  root_cause_proven=False, dma_retirement_proven=False,
                  recovery_authorized=False)
    if not lines:
        return result
    known = ('wifi-early5:', 'wifi-early5-boundary:', 'wifi-early5-publish:',
             'wifi-early5-boot:', 'wifi-early5-stop:')
    require(all(l.startswith(known) for l in lines), 'unknown early5 row')
    if not summaries:
        require(lines == ['wifi-early5-boot: calibration_only=1 net_init_deferred=1 worker_deferred=1'],
                'incomplete early5 owner rows')
        return dict(result, boot_requested=True, owner_observation_absent=True)
    require(len(summaries) == 1, 'missing early5 summary')
    s = row(summaries[0], 'wifi-early5', SUMMARY)
    require(all(s[k] in (0, 1) for k in ('complete', 'exhausted', 'calibration',
                'mac', 'stopped', 'frozen')), 'invalid early5 Boolean')
    require(s['retained'] == 1 and s['reused'] == s['inspected'] == s['network_out'] == 0,
            'early5 claims forbidden reuse/inspection/network')
    require(s['completed'] <= s['posted'] <= 32 and s['commands'] <= 5 and s['fault'] <= 7,
            'invalid early5 capacity')
    require(s['completed'] == s['during_mcu'] + s['during_control'] + s['idle_events'],
            'early5 completion accounting mismatch')
    require(s['exhausted'] == (s['completed'] == 32), 'early5 exhaustion mismatch')
    require(not s['complete'] or (s['posted'] == 32 and s['calibration'] and
            s['commands'] == 5 and s['mac'] and s['stopped'] and s['frozen'] and
            not s['fault']), 'incomplete early5 claims complete')
    require(not s['frozen'] or (s['stopped'] and not s['fault']), 'invalid early5 freeze')
    stops = [l for l in lines if l.startswith('wifi-early5-stop:')]
    require(len(stops) <= 1 and (not s['complete'] or stops), 'missing/duplicate early5 Stop')
    if stops:
        m = re.fullmatch(r'wifi-early5-stop: version=1 ok=([01]) state=([0-7]) dequeue=(0x[0-9a-f]{16}) ring=(0x[0-9a-f]{16}) completed=([0-9]+) posted=([0-9]+) event_seen=([01]) event_parameter=(0x[0-9a-f]{16}) event_status=(0x[0-9a-f]{16}) event_control=(0x[0-9a-f]{16}) retained=1', stops[0])
        require(m is not None, 'invalid early5 Stop schema')
        ok, state, dequeue, ring, completed, posted, seen, pointer, status, control = [int(x, 0) if x.startswith('0x') else int(x) for x in m.groups()]
        require((completed, posted) == (s['completed'], s['posted']), 'early5 Stop accounting mismatch')
        require(not s['complete'] or ok, 'early5 complete without successful Stop')
        if ok:
            published = [l for l in lines if l.startswith('wifi-early5-publish:')]
            require(len(published) == 1, 'early5 Stop lacks unique publication')
            binding = row(published[0], 'wifi-early5-publish',
                          'version pages slot ring before_mac retained reused'.split())
            require(state == 3 and seen and ring and ring % 4096 == 0 and
                    pointer == ring + 16 * completed and dequeue == (pointer | 1) and
                    ring == binding['ring'] and control >> 24 == binding['slot'] and
                    control >> 24 in range(1, 256) and (control >> 16) & 31 == 9 and
                    (control >> 10) & 63 == 32 and not control & ~(1 | (63 << 10) | (31 << 16) | (255 << 24)),
                    'early5 invalid Stop event/context')
            cc, residue = status >> 24, status & 0xffffff
            require(cc in (26, 27, 28) and
                    ((cc == 27 and residue == 0) or (cc != 27 and completed < posted and residue <= 4096)),
                    'early5 invalid Stop length')
        require(lines.index(stops[0]) < lines.index(summaries[0]), 'early5 Stop after summary')
    b = [row(l, 'wifi-early5-boundary', BOUNDARY) for l in lines
         if l.startswith('wifi-early5-boundary:')]
    require(len(b) == 5 and [x['step'] for x in b] == list(range(5)),
            'missing/duplicate/reordered early5 boundaries')
    previous = 0
    for i, x in enumerate(b):
        before, after = x['completed_before'], x['completed_after']
        if i < s['commands']:
            require(previous <= before <= after <= s['completed'], 'early5 boundary reversal')
            previous = after
        else:
            require(before == after == 0, 'unattempted early5 command contains data')
    publishes = [l for l in lines if l.startswith('wifi-early5-publish:')]
    require(len(publishes) == bool(s['posted']), 'early5 publication binding')
    if publishes:
        p = row(publishes[0], 'wifi-early5-publish',
                'version pages slot ring before_mac retained reused'.split())
        require(p['slot'] in range(1, 256) and p['ring'] and p['ring'] % 4096 == 0 and
                p == dict(version=1, pages=s['posted'], slot=p['slot'], ring=p['ring'],
                          before_mac=1, retained=1, reused=0),
                'early5 publication was not before MAC/unique/retained')
        require(lines.index(publishes[0]) < lines.index(summaries[0]), 'late early5 publication row')
    boots = [l for l in lines if l.startswith('wifi-early5-boot:')]
    require(len(boots) <= 1 and all(l == 'wifi-early5-boot: calibration_only=1 net_init_deferred=1 worker_deferred=1' for l in boots),
            'invalid early5 boot row')
    require([l.split(':', 1)[0] for l in lines[lines.index(summaries[0]):]] ==
            ['wifi-early5'] + ['wifi-early5-boundary'] * 5, 'early5 terminal row order')
    loss = observe_loss(log)
    require(len(loss['groups']) <= s['commands'], 'too many early5 MCU receives')
    if s['calibration']:
        require(len(loss['groups']) == 5 and all(g['summary']['step'] == i and
                g['summary']['original_reason'] == 2 and g['summary']['original_result'] == 16
                for i, g in enumerate(loss['groups'])), 'early5 calibration lacks five successful native MCU observations')
    result.update(summary=s, boundaries=b, loss_trace=loss,
                  calibration_queue_exhausted=any(x['completed_after'] == 32 for x in b[:s['commands']]))
    return result
