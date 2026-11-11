"""Bind the worst private wait to actual I/O ownership and USB status callbacks.

Owner metadata comes from existing scheduler diagnostic edges. USB samples
reuse already-timed callbacks. Software observation is not device arrival time.
"""
import re

FIELDS = {
 'wait': 'enabled invalid active waits pid start_us end_us target base live request borrowed retry open_at_end calls overlap_us parent_pid parent_id identity slot size content_start content_end ok dirty_after overlap_start overlap_end owner_calls owned_us',
 'owner': 'pid seq thread owner_pid syscall name start_us end_us released overlap_start overlap_end csws csw_us status_overlap_us over100ms write512 write512_us write16k write16k_us',
 'status': 'pid tag start_us end_us opcode command_bytes requested actual bulk_ok submitted observed resumed seen service_valid checks samples service_gap completion_gap empty_checks event_checks last_empty',
 'io': 'pid valid io_us payload_us alloc_read_us alloc_write_us prefix_us sidecar_us failures usb_valid usb_invalid usb_active csws csw_us over100ms payload_commands payload_bytes payload_csw_us payload_long',
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
            pattern = ' '.join(k + (r'=([!-~]{1,31})' if k == 'name' else r'=([0-9]+)') for k in fields)
            m = re.fullmatch(pattern, values)
            if not m:
                raise ValueError('Malformed commit ' + kind)
            row = {k: v if k == 'name' else int(v) for k, v in zip(fields, m.groups())}
            if any(v >= 2**64 for k, v in row.items() if k != 'name') or row['version'] != 2 or row['final'] != 1 or not row['snapshot_us']:
                raise ValueError('Overflowing/unsupported/nonfinal commit observation')
            group = groups.setdefault(row['snapshot_us'], {})
            if kind in group and group[kind] != row:
                raise ValueError('Conflicting commit companion')
            group[kind] = row
    at = writes['snapshot_us']
    if queued['snapshot_us'] != at or at not in groups or not {'wait', 'owner'} <= set(groups[at]):
        raise ValueError('Missing bound wait/owner header')
    g = groups[at]; w = g['wait']; o = g['owner']
    if (w['enabled'], w['invalid'], w['active']) != (1, 0, 0) or any(r['pid'] != w['pid'] for r in g.values()):
        raise ValueError('Invalid/inflight/unbound observation')
    maximum = max((r['max_us'] for r in queued['workers']), default=0)
    if w['waits'] != sum(r['calls'] for r in queued['workers']):
        raise ValueError('Unbound total wait count')
    if not w['pid']:
        if maximum or w['waits'] or any(w[k] for k in FIELDS['wait'].split() if k != 'enabled'):
            raise ValueError('Unmarked absent wait')
    else:
        q = [r for r in queued['workers'] if r['pid'] == w['pid']]
        if len(q) != 1 or not w['target'] or not 0 < w['start_us'] <= w['end_us'] <= at or w['end_us']-w['start_us'] != q[0]['max_us'] or q[0]['max_us'] != maximum:
            raise ValueError('Wait is not the exact aggregate maximum')
        if not (0 <= w['base'] <= w['live'] <= 16*1024*1024 and 0 < w['request'] <= 65536 and w['live']+w['request'] <= 16*1024*1024 and w['live']+w['request']-w['base'] > 262144 and w['borrowed'] <= 1):
            raise ValueError('Target not in original bounded full-window state')
        if w['retry'] > 1 or w['open_at_end'] > 1 or max(w['overlap_us'], w['owned_us']) > maximum:
            raise ValueError('Invalid wait flags or coverage')
    # Completed content is optional and independent of actual owner selection.
    if w['calls']:
        if not (w['parent_pid'] and w['identity'] and 0 < w['parent_id'] <= w['content_start'] < w['content_end'] <= w['end_us'] and w['overlap_start'] == max(w['start_us'], w['content_start']) and w['overlap_end'] == w['content_end'] and 0 < w['overlap_end']-w['overlap_start'] <= w['overlap_us'] <= w['calls']*(w['overlap_end']-w['overlap_start'])):
            raise ValueError('Invalid completed content intersection')
        if w['ok'] != 1 or w['dirty_after'] > 1 or w['size'] > 16*1024*1024 or 'io' not in g:
            raise ValueError('Failed/missing completed content')
        io = g['io']; span = w['content_end']-w['content_start']
        if (io['valid'], io['failures'], io['usb_valid'], io['usb_invalid'], io['usb_active']) != (1, 0, 1, 0, 0):
            raise ValueError('Invalid content counters')
        if io['io_us'] > span or sum(io[k] for k in ['payload_us', 'alloc_read_us', 'alloc_write_us', 'prefix_us', 'sidecar_us']) > io['io_us'] or io['csw_us'] > span or io['over100ms'] > io['csws'] or io['over100ms'] and io['over100ms']*100000 >= io['csw_us']:
            raise ValueError('Inconsistent whole-content durations')
        if io['payload_commands'] > io['csws'] or io['payload_bytes'] > io['payload_commands']*16384 or io['payload_csw_us'] > min(io['csw_us'], io['payload_us']) or io['payload_long'] > min(io['payload_commands'], io['over100ms']):
            raise ValueError('Unbound payload subset')
    else:
        zero = 'overlap_us parent_pid parent_id identity slot size content_start content_end ok dirty_after overlap_start overlap_end'.split()
        if 'io' in g or any(w[k] for k in zero):
            raise ValueError('Invented completed content')
    base = {'wait', 'owner'} | ({'io'} if w['calls'] else set())
    result = dict(snapshot_us=at, wait=w, owner=o, io=g.get('io'),
                  scope='Actual I/O-owner intersection; whole USB callback clocks can start before wait. Completion is software observation, not hardware arrival or command validation.')
    if not o['thread']:
        if set(g) != base or o['name'] != 'none' or any(o[k] for k in FIELDS['owner'].split() if k not in ('name', 'pid')) or w['owner_calls'] or w['owned_us']:
            raise ValueError('Unmarked absent I/O owner')
        return dict(result, status='VALID_UNEXERCISED' if not w['pid'] else 'VALID_NO_OWNER')
    if not w['pid'] or not o['seq'] or o['released'] > 1 or not 0 < o['start_us'] <= o['overlap_start'] < o['overlap_end'] <= w['end_us']:
        raise ValueError('Invalid owner identity/clock')
    if o['overlap_start'] != max(w['start_us'], o['start_us']):
        raise ValueError('Unbound owner intersection')
    if (o['released'] and o['end_us'] != o['overlap_end']) or (not o['released'] and (o['end_us'] or o['overlap_end'] != w['end_us'])):
        raise ValueError('Invented owner release')
    owned = o['overlap_end']-o['overlap_start']
    if not 0 < owned <= w['owned_us'] <= min(maximum, w['owner_calls']*owned):
        raise ValueError('Invalid longest-owner coverage')
    result.update(uncovered_wait_us=maximum-w['owned_us'])
    if not o['csws']:
        if set(g) != base or any(o[k] for k in 'csw_us status_overlap_us over100ms write512 write512_us write16k write16k_us'.split()):
            raise ValueError('Unmarked absent owner status')
        return dict(result, status='VALID_OWNER_NO_STATUS')
    if set(g) != base | {'status'}:
        raise ValueError('Missing/extra USB status companion')
    s = g['status']; duration = s['end_us']-s['start_us']
    clipped = max(0, min(w['end_us'], s['end_us'])-max(w['start_us'], s['start_us']))
    if not o['start_us'] <= s['start_us'] <= s['end_us'] <= o['overlap_end'] or not 0 < clipped <= o['status_overlap_us'] <= min(owned, o['csws']*clipped):
        raise ValueError('Status not within exact owner/wait')
    if not o['status_overlap_us'] <= o['csw_us'] <= o['overlap_end']-o['start_us'] or o['over100ms'] > o['csws'] or o['over100ms'] and o['over100ms']*100000 >= o['csw_us']:
        raise ValueError('Invalid owner status totals')
    if o['write512']+o['write16k'] > o['csws'] or o['write512_us']+o['write16k_us'] > o['csw_us'] or (not o['write512'] and o['write512_us']) or (not o['write16k'] and o['write16k_us']):
        raise ValueError('Invalid 512-byte/16KiB subsets')
    if (s['requested'], s['actual'], s['bulk_ok'], s['seen'], s['service_valid']) != (13, 13, 1, 1, 1) or s['tag'] >= 2**32:
        raise ValueError('Invalid status callback result/tag')
    if s['command_bytes'] > 16384 or s['command_bytes']%512 or s['opcode'] not in (0x28, 0x2a, 0x35) or (s['opcode']==0x35) != (s['command_bytes']==0):
        raise ValueError('Changed command size/opcode')
    if not s['start_us'] <= s['submitted'] <= s['observed'] <= s['resumed'] <= s['end_us'] or s['empty_checks']+s['event_checks'] != s['checks'] or not s['event_checks'] or not s['checks'] or s['samples'] != (s['checks']-1)//256+1 or not s['completion_gap'] <= s['service_gap'] <= s['observed']-s['submitted'] <= (s['samples']+1)*s['service_gap']:
        raise ValueError('Invalid dispatch/service observation')
    if s['last_empty'] and (not s['empty_checks'] or not s['submitted'] <= s['last_empty'] <= s['observed']):
        raise ValueError('Invalid empty-ring sample')
    return dict(result, status='VALID', csw=s, csw_wait_overlap_us=clipped)
