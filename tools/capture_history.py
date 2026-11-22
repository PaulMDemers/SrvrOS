"""Strict bounded three-entry final capture-completion history."""
import re

_HEADER = 'version snapshot_us completed count dropped invalid final'.split()
_FIELDS = ('version snapshot_us index id pid started_us finished_us retired_us result status ' +
           ' '.join('p%d_us' % n for n in range(1, 13)) + ' final').split()
_U64 = 1 << 64


def _parse(line, fields):
    text = line.partition(': ')[2]
    match = re.fullmatch(' '.join(k + (r'=(-?\d+)' if k == 'result' else r'=(\d+)') for k in fields), text)
    if not match:
        raise ValueError('Malformed capture history record')
    row = dict(zip(fields, map(int, match.groups())))
    if any(v >= _U64 for k, v in row.items() if k != 'result'):
        raise ValueError('Overflowing capture history value')
    if row['version'] != 1 or not row['snapshot_us'] or row['final'] not in (0, 1):
        raise ValueError('Unsupported capture history observation')
    return row


def capture_history(blobs, expected=3):
    if type(expected) is not int or not 0 <= expected <= 3:
        raise ValueError('Capture history expectation exceeds retained capacity')
    groups = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith(('desktop-capture-history:', 'desktop-capture-completion:')):
                continue
            header = line.startswith('desktop-capture-history:')
            row = _parse(line, _HEADER if header else _FIELDS)
            g = groups.setdefault(row['snapshot_us'], {'header': {}, 'rows': {}})
            table, key = (g['header'], 0) if header else (g['rows'], row['index'])
            if key in table and table[key] != row:
                raise ValueError('Conflicting capture history record')
            table[key] = row
    finals = [(at, g) for at, g in groups.items() if g['header'].get(0, {}).get('final') == 1]
    if not finals:
        raise ValueError('Missing final capture history')
    at, g = max(finals, key=lambda item: item[0])
    h = g['header'][0]
    if h['dropped'] or h['invalid'] or h['completed'] != expected or h['count'] != expected:
        raise ValueError('Incomplete/dropped capture history')
    if set(g['rows']) != set(range(expected)):
        raise ValueError('Missing/extra capture completion')
    captures = []
    ids = set()
    pids = set()
    previous_id = 0
    previous_retired = 0
    for index in range(expected):
        r = g['rows'][index]
        if r['final'] != 1 or r['result'] != 1 or r['status'] != 0:
            raise ValueError('Nonfinal/failed capture completion')
        if not 0 < r['id'] <= 0xffffffff or r['id'] <= previous_id or r['id'] in ids or not r['pid'] or r['pid'] in pids:
            raise ValueError('Invalid/repeated capture identity')
        if not (0 < r['started_us'] <= r['finished_us'] <= r['retired_us'] <= at):
            raise ValueError('Invalid capture lifecycle clock')
        if r['started_us'] < previous_retired:
            raise ValueError('Overlapping capture ownership lifetimes')
        total = sum(r['p%d_us' % n] for n in range(1, 13))
        duration = r['finished_us'] - r['started_us']
        if total > duration:
            raise ValueError('Capture phases exceed elapsed lifetime')
        ids.add(r['id']); pids.add(r['pid'])
        previous_id = r['id']; previous_retired = r['retired_us']
        captures.append(dict(r, duration_us=duration, phase_us=total))
    durations = sorted(r['duration_us'] for r in captures)
    return dict(status='VALID', snapshot_us=at, completed=expected, captures=captures,
                median_us=durations[len(durations)//2] if durations else None,
                max_us=max(durations) if durations else None,
                scope='All retained capture completion lifetimes and worker phases; not CPU or hardware time.')
