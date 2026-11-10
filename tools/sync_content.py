"""Strict final SYS_SYNC/content joins; counters are software wall observations."""
import re
from sync_timing import sync_timing

_FIELDS = {
    'summary': 'version snapshot_us pid id calls bytes failures final',
    'selection': 'version snapshot_us pid id final',
    'worst': 'version snapshot_us pid id slot identity inode size start_us end_us ok dirty_after final',
    'prefix': 'version snapshot_us pid id identity start_generation end_generation live_size committed_size append_calls append_bytes final',
    'io': 'version snapshot_us pid id role reads read_bytes read_us writes write_bytes write_us flushes flush_us failures final',
}
_U64 = 1 << 64
_CONTENTS = 192  # PROCESS_MAX_WRITE_FILES + PROCESS_MAX_READ_FILES.
_BUFFER = 16 * 1024 * 1024  # PROCESS_WRITE_BUFFER_MAX, unchanged.


def _row(line):
    name, sep, text = line.partition(': ')
    kind = name.removeprefix('sync-content-')
    if not sep or kind not in _FIELDS:
        raise ValueError('Malformed sync content prefix')
    fields = _FIELDS[kind].split()
    match = re.fullmatch(' '.join(k + r'=(\d+)' for k in fields), text)
    if not match:
        raise ValueError('Malformed sync content ' + kind)
    row = dict(zip(fields, map(int, match.groups())))
    if any(v >= _U64 for v in row.values()):
        raise ValueError('Overflowing sync content value')
    versions = (1, 2) if kind == 'worst' else (1,)
    if row['version'] not in versions or row['final'] != 1 or not row['snapshot_us']:
        raise ValueError('Unsupported/nonfinal sync content observation')
    return kind, row


def sync_content(blobs, sync_observed=None, require_work=True):
    """Join to the same complete final sync snapshot and selected worst call.

    The selected sync has the largest file phase among retained per-PID worst
    calls containing dirty-content visits. Ties use printed summary order.
    It is not necessarily the globally slowest individual content flush.
    """
    groups = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('sync-content-'):
                continue
            kind, row = _row(line)
            g = groups.setdefault(row['snapshot_us'], {'summary': {}, 'selection': {}, 'worst': {}, 'io': {}, 'prefix': {}})
            key = row['pid'] if kind == 'summary' else row['role'] if kind == 'io' else 0
            table = g[kind]
            if key in table and table[key] != row:
                raise ValueError('Conflicting sync content ' + kind)
            table[key] = row
    observed = sync_timing(blobs) if sync_observed is None else sync_observed
    if observed.get('status') != 'VALID' or observed.get('final', 1) != 1:
        raise ValueError('Invalid joined sync observation')
    at = observed['snapshot_us']
    g = groups.get(at)
    if g is None or set(g['selection']) != {0}:
        raise ValueError('Missing bound sync content selection')
    workers = {w['pid']: w for w in observed['workers']}
    if len(workers) != len(observed['workers']) or not 1 <= len(workers) <= 8:
        raise ValueError('Invalid joined sync workers')
    summaries = g['summary']
    if set(summaries) != set(workers):
        raise ValueError('Missing/extra sync content summaries')
    candidates = []
    for pid, row in summaries.items():
        w = workers[pid]['worst']
        if not pid or row['id'] != w['start_us'] or row['calls'] > _CONTENTS:
            raise ValueError('Unbound sync content summary')
        if row['failures'] or row['bytes'] > row['calls'] * _BUFFER:
            raise ValueError('Failed/invalid content summary')
        if not row['calls']:
            if row['bytes']:
                raise ValueError('Bytes without dirty content')
        else:
            candidates.append((pid, row['id'], w['files_end_us'] - w['start_us']))
    selection = g['selection'][0]
    chosen = max(candidates, key=lambda c: c[2]) if candidates else (0, 0, 0)
    if (selection['pid'], selection['id']) != chosen[:2]:
        raise ValueError('Selection is not the joined longest content-bearing sync')
    if not candidates:
        if require_work or g['worst'] or g['io'] or g['prefix']:
            raise ValueError('Missing work or unmarked content details')
        return dict(status='VALID', snapshot_us=at, selection=selection,
                    summaries=list(summaries.values()), worst=None, io=[])
    if set(g['worst']) != {0} or set(g['io']) != set(range(6)):
        raise ValueError('Missing/extra selected content detail')
    worst = g['worst'][0]
    identity = (selection['pid'], selection['id'])
    if (worst['pid'], worst['id']) != identity:
        raise ValueError('Unbound selected content')
    w = workers[selection['pid']]['worst']
    if not (w['start_us'] <= worst['start_us'] <= worst['end_us'] <= w['files_end_us'] <= at):
        raise ValueError('Content outside its selected sync file phase')
    if not worst['start_us'] or not worst['identity'] or worst['slot'] >= _CONTENTS or worst['size'] > _BUFFER:
        raise ValueError('Invalid content lifetime/size')
    if worst['ok'] != 1 or worst['size'] > summaries[worst['pid']]['bytes']:
        raise ValueError('Failed or inconsistent selected content')
    prefix = None
    if worst['version'] == 1:
        if worst['dirty_after'] != 0 or g['prefix']:
            raise ValueError('Failed/dirty legacy content or unmarked prefix detail')
    else:
        if set(g['prefix']) != {0}:
            raise ValueError('Missing/extra captured content prefix')
        prefix = g['prefix'][0]
        if (prefix['pid'], prefix['id'], prefix['identity']) != (*identity, worst['identity']):
            raise ValueError('Unbound captured content prefix')
        start, end = prefix['start_generation'], prefix['end_generation']
        calls, added = prefix['append_calls'], prefix['append_bytes']
        if (end < start or prefix['live_size'] < worst['size'] or
                prefix['live_size'] > _BUFFER or prefix['committed_size'] != worst['size'] or
                calls != end - start or added != prefix['live_size'] - worst['size'] or
                bool(calls) != bool(added) or (added and (added - 1) // 65536 >= calls) or
                worst['dirty_after'] != int(bool(calls))):
            raise ValueError('Inconsistent/uncommitted captured content prefix')
    elapsed = worst['end_us'] - worst['start_us']
    total_us = 0
    operations = 0
    rows = []
    for role in range(6):
        r = g['io'][role]
        if (r['pid'], r['id']) != identity or r['failures']:
            raise ValueError('Unbound/failed content I/O')
        for count, byte, clock in [('reads', 'read_bytes', 'read_us'), ('writes', 'write_bytes', 'write_us')]:
            if not r[count] and (r[byte] or r[clock]):
                raise ValueError('Content byte/time without operations')
        if not r['flushes'] and r['flush_us']:
            raise ValueError('Flush time without flush')
        total_us += r['read_us'] + r['write_us'] + r['flush_us']
        operations += r['reads'] + r['writes'] + r['flushes']
        rows.append(r)
    if total_us > elapsed or (require_work and not operations):
        raise ValueError('Overlapping/absent content I/O observations')
    result = dict(status='VALID', snapshot_us=at, selection=selection,
                summaries=list(summaries.values()), worst=worst, io=rows,
                io_us=total_us, residual_us=elapsed-total_us,
                scope='Selected retained worst sync and its slowest content subcall; role block callbacks are disjoint software wall observations, not hardware or CPU time.')
    if prefix is not None:
        result['prefix'] = prefix
    return result
