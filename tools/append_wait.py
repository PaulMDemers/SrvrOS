"""Final-only queued private-append wait wall; no synthetic storage owner."""
import re

def append_wait(blobs, writes):
    fields = 'version snapshot_us pid calls wall_us max_us wakes overlap_ready final'.split()
    pattern = re.compile('capture-append-wait: ' + ' '.join(k + r'=(\d+)' for k in fields))
    at = writes['snapshot_us']
    rows = {}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('capture-append-wait:'):
                continue
            match = pattern.fullmatch(line)
            if not match:
                raise ValueError('Malformed append wait companion')
            row = dict(zip(fields, map(int, match.groups())))
            if any(v >= 2**64 for v in row.values()) or row['version'] != 1 or row['final'] != 1:
                raise ValueError('Unsupported append wait companion')
            if row['snapshot_us'] != at:
                continue
            pid = row['pid']
            if pid in rows and rows[pid] != row:
                raise ValueError('Conflicting append wait companion')
            rows[pid] = row
    workers = writes['workers']
    if set(rows) != {w['record']['pid'] for w in workers}:
        raise ValueError('Missing or extra append wait companion')
    for worker in workers:
        r = worker['record']; q = rows[r['pid']]; call = worker['sites']['call']
        if q['calls'] > r['calls'] or q['overlap_ready'] > min(q['calls'], r['fast_calls'], q['wakes']):
            raise ValueError('Unbound append wait count')
        if q['wall_us'] > call['total_us'] or q['max_us'] > min(q['wall_us'], call['max_us']):
            raise ValueError('Append wait outside raw call wall')
        if not q['calls']:
            if any(q[k] for k in ('wall_us', 'max_us', 'wakes', 'overlap_ready')):
                raise ValueError('Append wait work without calls')
        elif not 0 < q['max_us'] <= q['wall_us'] <= q['calls'] * q['max_us']:
            raise ValueError('Inconsistent append wait wall')
    return dict(status='VALID', snapshot_us=at, workers=[rows[w['record']['pid']] for w in workers],
        overlap_ready=sum(q['overlap_ready'] for q in rows.values()),
        scope='Private prelude wait wall retained explicitly, including fallback preludes outside ordinary handler. No grant of filesystem ownership or hardware completion attribution.')
