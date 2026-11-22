"""Validated BOT command context bound to the selected actual-owner status.

Phase clocks are optional. No global command maximum or PID proximity join.
"""
import re

FIELDS = ('version snapshot_us pid available phases tag opcode lba blocks sector flags '
          'valid complete ok role start_us end_us csw_start csw_end cbw_calls cbw_us '
          'data_calls data_req data_actual data_us csw_calls csw_us ck_calls ck_us final').split()


def commit_command(blobs, isolation):
    rows = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('capture-owner-command:'):
                continue
            m = re.fullmatch('capture-owner-command: ' + ' '.join(k+r'=(\d+)' for k in FIELDS), line)
            if not m:
                raise ValueError('Malformed owner command')
            r = dict(zip(FIELDS, map(int, m.groups())))
            if any(v >= 2**64 for v in r.values()) or r['version'] != 1 or r['final'] != 1 or not r['snapshot_us']:
                raise ValueError('Unsupported/overflowing/nonfinal owner command')
            at = r['snapshot_us']
            if at in rows and rows[at] != r:
                raise ValueError('Conflicting owner command')
            rows[at] = r
    at = isolation['snapshot_us']
    if at not in rows:
        raise ValueError('Missing exact owner command observation')
    r = rows[at]; w = isolation['wait']; o = isolation['owner']; s = isolation.get('csw')
    if r['pid'] != w['pid'] or r['tag'] != (s['tag'] if s else 0):
        raise ValueError('Unbound owner command identity')
    if not r['available']:
        if any(v for k, v in r.items() if k not in ('version', 'snapshot_us', 'pid', 'tag', 'final')):
            raise ValueError('Invented unavailable command')
        return dict(status='VALID_UNAVAILABLE', command=r)
    if r['available'] != 1 or r['phases'] not in (0, 1) or not s or isolation['status'] != 'VALID':
        raise ValueError('Invalid command availability')
    if (r['valid'], r['complete'], r['ok']) != (1, 1, 1):
        raise ValueError('Failed/incomplete original command validation')
    if (r['opcode'], r['csw_start'], r['csw_end']) != (s['opcode'], s['start_us'], s['end_us']):
        raise ValueError('Command is not the exact selected status callback')
    if r['flags'] or r['sector'] not in (512, 1024, 2048, 4096) or r['lba'] >= 2**32 or r['blocks'] >= 2**16 or r['lba']+r['blocks'] > 2**32:
        raise ValueError('Changed command flags or invalid block range')
    if r['opcode'] == 0x35:
        if r['lba'] or r['blocks'] or s['command_bytes']:
            raise ValueError('Invalid flush CDB context')
    elif r['opcode'] not in (0x28, 0x2a) or not r['blocks'] or r['blocks']*r['sector'] != s['command_bytes']:
        raise ValueError('Command block range does not match exact request bytes')
    phase_fields = 'start_us end_us cbw_calls cbw_us data_calls data_req data_actual data_us csw_calls csw_us ck_calls ck_us'.split()
    if not r['phases']:
        if r['role'] != 7 or any(r[k] for k in phase_fields):
            raise ValueError('Invented uncollected command phases')
        return dict(status='VALID_COMMAND_ONLY', command=r)
    if r['role'] >= 7 or not o['start_us'] <= r['start_us'] <= r['csw_start'] <= r['csw_end'] <= r['end_us'] <= o['overlap_end']:
        raise ValueError('Command phases outside exact owner')
    count = (s['command_bytes']+4095)//4096
    if r['cbw_calls'] != 1 or r['csw_calls'] != 1 or r['data_calls'] != count or r['data_req'] != s['command_bytes'] or r['data_actual'] != s['command_bytes'] or r['ck_calls'] != max(1, count):
        raise ValueError('Changed command callback/checkpoint counts or byte proof')
    if r['csw_us'] != s['end_us']-s['start_us'] or r['cbw_us']+r['data_us'] > r['csw_start']-r['start_us'] or sum(r[k] for k in ('cbw_us', 'data_us', 'csw_us', 'ck_us')) > r['end_us']-r['start_us']:
        raise ValueError('Inconsistent existing command phase clocks')
    if not count and r['data_us']:
        raise ValueError('Invented flush data phase')
    return dict(status='VALID_COMPLETE_PHASES', command=r,
                residual_us=r['end_us']-r['start_us']-sum(r[k] for k in ('cbw_us', 'data_us', 'csw_us', 'ck_us')))
