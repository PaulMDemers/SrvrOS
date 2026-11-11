"""Strict completed-commit intersection with the worst private append wait.

Counters cover the whole selected call, not exclusive wait work or hardware
latency. The compact final report does not replace any legacy contract.
"""
import re

FIELDS = {
 'wait': 'enabled invalid active waits pid start_us end_us target base live request borrowed retry open_at_end calls overlap_us parent_pid parent_id identity slot size content_start content_end ok dirty_after overlap_start overlap_end',
 'io': 'pid valid io_us payload_us alloc_read_us alloc_write_us prefix_us sidecar_us failures usb_valid usb_invalid usb_active csws csw_us over100ms payload_commands payload_bytes payload_csw_us payload_long',
 'command': 'pid valid complete ok role start_us end_us csw_start csw_end checkpoint_calls checkpoint_us checkpoint_first checkpoint_last',
 'csw': 'pid start_us end_us opcode command_bytes requested actual ok submitted observed resumed seen service_valid checks samples service_gap completion_gap empty_checks event_checks last_empty',
 'phase': 'pid phase calls requested actual us first_us last_us',
}

def commit_wait(blobs, writes, queued):
    groups = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('capture-commit-'):
                continue
            name, sep, values = line.partition(': ')
            kind = name.removeprefix('capture-commit-')
            if not sep or kind not in FIELDS:
                raise ValueError('Unknown/malformed commit wait companion')
            fields = ('version snapshot_us ' + FIELDS[kind] + ' final').split()
            m = re.fullmatch(' '.join(k + r'=([0-9]+)' for k in fields), values)
            if not m:
                raise ValueError('Malformed commit ' + kind)
            row = dict(zip(fields, map(int, m.groups())))
            if any(v >= 2**64 for v in row.values()) or row['version'] != 1 or row['final'] != 1 or not row['snapshot_us']:
                raise ValueError('Overflowing/unsupported/nonfinal commit observation')
            group = groups.setdefault(row['snapshot_us'], {})
            key = (kind, row.get('phase', 0))
            if key in group and group[key] != row:
                raise ValueError('Conflicting commit companion')
            group[key] = row
    at = writes['snapshot_us']
    if queued['snapshot_us'] != at or at not in groups or ('wait', 0) not in groups[at]:
        raise ValueError('Missing bound commit wait header')
    g = groups[at]; w = g['wait', 0]
    if (w['enabled'], w['invalid'], w['active']) != (1, 0, 0):
        raise ValueError('Invalid/inflight commit wait observation')
    maximum = max((r['max_us'] for r in queued['workers']), default=0)
    if w['waits'] != sum(r['calls'] for r in queued['workers']):
        raise ValueError('Unbound total observed wait count')
    if not w['pid']:
        if maximum or w['waits'] or set(g) != {('wait', 0)} or any(w[k] for k in FIELDS['wait'].split() if k != 'enabled'):
            raise ValueError('Unmarked absent wait')
        return dict(status='VALID_UNEXERCISED', snapshot_us=at, wait=w)
    q = [r for r in queued['workers'] if r['pid'] == w['pid']]
    if len(q) != 1 or not w['target'] or not 0 < w['start_us'] <= w['end_us'] <= at or w['end_us']-w['start_us'] != q[0]['max_us'] or q[0]['max_us'] != maximum:
        raise ValueError('Wait does not equal exact aggregate maximum')
    if not (0 <= w['base'] <= w['live'] <= 16*1024*1024 and 0 < w['request'] <= 65536 and w['live']+w['request'] <= 16*1024*1024 and w['live']+w['request']-w['base'] > 262144 and w['borrowed'] <= 1):
        raise ValueError('Target was not in the original bounded full-window state')
    if w['retry'] > 1 or w['open_at_end'] > 1 or w['overlap_us'] > maximum:
        raise ValueError('Invalid wait flags or overlap total')
    if not w['calls']:
        zero = 'overlap_us parent_pid parent_id identity slot size content_start content_end ok dirty_after overlap_start overlap_end'.split()
        if set(g) != {('wait', 0)} or any(w[k] for k in zero):
            raise ValueError('Content data without completed overlap')
        return dict(status='VALID_NO_COMPLETED_OVERLAP', snapshot_us=at, wait=w)
    base = {('wait', 0), ('io', 0), ('command', 0)}
    if not base <= set(g) or any(r['pid'] != w['pid'] for r in g.values()):
        raise ValueError('Missing/extra/unbound commit companions')
    if not (w['parent_pid'] and w['identity'] and 0 < w['parent_id'] <= w['content_start'] < w['content_end'] <= w['end_us'] <= at and w['overlap_start'] == max(w['start_us'], w['content_start']) and w['overlap_end'] == w['content_end'] and 0 < w['overlap_end']-w['overlap_start'] <= w['overlap_us'] <= w['calls']*(w['overlap_end']-w['overlap_start'])):
        raise ValueError('Invalid exact content intersection/longest selection')
    if w['ok'] != 1 or w['dirty_after'] > 1 or w['size'] > 16*1024*1024:
        raise ValueError('Failed/invalid selected content')
    io = g['io', 0]; c = g['command', 0]
    span = w['content_end']-w['content_start']
    if (io['valid'], io['failures'], io['usb_valid'], io['usb_invalid'], io['usb_active']) != (1, 0, 1, 0, 0):
        raise ValueError('Invalid content/USB counters')
    if not io['csws']:
        zero = 'csw_us over100ms payload_commands payload_bytes payload_csw_us payload_long'.split()
        command_zero = set(c)-{'version', 'snapshot_us', 'pid', 'final'}
        if set(g) != base or any(io[k] for k in zero) or any(c[k] for k in command_zero) or io['io_us'] > span:
            raise ValueError('Unmarked absent USB command')
        return dict(status='VALID_NO_USB_COMMAND', snapshot_us=at, wait=w, io=io, command=c)
    expected = base | {('csw', 0), *(('phase', i) for i in range(3))}
    if set(g) != expected:
        raise ValueError('Missing/extra USB command companions')
    s = g['csw', 0]
    if io['io_us'] > span or sum(io[k] for k in ['payload_us', 'alloc_read_us', 'alloc_write_us', 'prefix_us', 'sidecar_us']) > io['io_us'] or not 0 < io['csws'] or io['csw_us'] > span or io['over100ms'] > io['csws'] or io['over100ms']*100000 >= io['csw_us'] and io['over100ms']:
        raise ValueError('Inconsistent content/USB durations')
    if io['payload_commands'] > io['csws'] or io['payload_bytes'] > io['payload_commands']*16384 or io['payload_csw_us'] > min(io['csw_us'], io['payload_us']) or io['payload_long'] > min(io['payload_commands'], io['over100ms']):
        raise ValueError('Unbound payload subset')
    if (c['valid'], c['complete'], c['ok']) != (1, 1, 1) or c['role'] not in range(7):
        raise ValueError('Incomplete/failed command')
    if not (w['content_start'] <= c['start_us'] <= c['csw_start'] <= c['csw_end'] <= c['end_us'] <= w['content_end']) or (s['start_us'], s['end_us']) != (c['csw_start'], c['csw_end']):
        raise ValueError('Command/status not inside exact content')
    csw_us = s['end_us']-s['start_us']
    if not 0 < csw_us <= io['csw_us'] <= io['csws']*csw_us or (s['requested'], s['actual'], s['ok'], s['seen'], s['service_valid']) != (13, 13, 1, 1, 1):
        raise ValueError('Invalid retained maximum/status result')
    if s['command_bytes'] > 16384 or s['command_bytes']%512 or s['opcode'] not in (0x28, 0x2a, 0x35) or (s['opcode']==0x35) != (s['command_bytes']==0):
        raise ValueError('Changed command size/opcode')
    if not (s['start_us'] <= s['submitted'] <= s['observed'] <= s['resumed'] <= s['end_us']) or s['empty_checks']+s['event_checks'] != s['checks'] or not s['event_checks'] or s['samples'] > s['checks'] or s['service_gap'] > s['observed']-s['submitted'] or s['completion_gap'] > s['service_gap']:
        raise ValueError('Invalid dispatch/service observation')
    if s['last_empty'] and (not s['empty_checks'] or not s['submitted'] <= s['last_empty'] <= s['observed']):
        raise ValueError('Invalid empty-ring sample')
    phases = [g['phase', i] for i in range(3)]
    expected = [(1, 31), ((s['command_bytes']+4095)//4096, s['command_bytes']), (1, 13)]
    previous = c['start_us']
    for p, (calls, size) in zip(phases, expected):
        if (p['calls'], p['requested'], p['actual']) != (calls, size, size):
            raise ValueError('Changed phase segmentation/bytes')
        if not calls:
            if any(p[k] for k in ('us', 'first_us', 'last_us')): raise ValueError('Work on absent data phase')
            continue
        if not previous <= p['first_us'] <= p['last_us'] <= c['end_us'] or p['us'] > p['last_us']-p['first_us'] or calls==1 and p['us'] != p['last_us']-p['first_us']:
            raise ValueError('Invalid phase clock')
        previous = p['last_us']
    if phases[0]['first_us'] != c['start_us'] or (phases[2]['first_us'], phases[2]['last_us'], phases[2]['us']) != (s['start_us'], s['end_us'], csw_us):
        raise ValueError('Unbound exact phase endpoints')
    if c['checkpoint_calls'] != max(1, expected[1][0]) or not c['start_us'] <= c['checkpoint_first'] <= c['checkpoint_last'] == c['end_us'] or c['checkpoint_us'] > c['checkpoint_last']-c['checkpoint_first'] or sum(p['us'] for p in phases)+c['checkpoint_us'] > c['end_us']-c['start_us']:
        raise ValueError('Invalid command checkpoints/span')
    if c['checkpoint_calls']==1:
        if c['checkpoint_first'] < s['end_us'] or c['checkpoint_us'] != c['checkpoint_last']-c['checkpoint_first']: raise ValueError('Invalid final checkpoint')
    elif not phases[1]['first_us'] <= c['checkpoint_first'] <= phases[1]['last_us']:
        raise ValueError('Invalid inter-data checkpoint')
    return dict(status='VALID', snapshot_us=at, wait=w, io=io, command=c, csw=s, phases=phases,
                uncovered_wait_us=maximum-w['overlap_us'], csw_wait_overlap_us=max(0,min(w['end_us'],s['end_us'])-max(w['start_us'],s['start_us'])),
                scope='Completed overlapping content selected within exact worst private wait; whole-call counters, not exclusive blocked work, exact scheduler ownership or hardware latency')
