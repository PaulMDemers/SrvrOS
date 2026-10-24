"""Validate logger wall-time records wholly inside a completed idle window."""
import re

FIELDS = ('start_us end_us bytes prepare_us write_us verify_us sync_us '
          'cbw_calls cbw_us data_calls data_us csw_calls csw_us failures ok valid').split()
PATTERN = re.compile(r'^workspace-cost: version=1 name=(boot\.log|session-[ab]\.log|exit\.log) '
                     + ' '.join(k + r'=([0-9]{1,20})' for k in FIELDS) + '$')


def logger_cost(text, start, end):
    if not 0 <= start < end < 2**64:
        return None
    records = {}
    for line in text.splitlines():
        if not line.startswith('workspace-cost:'):
            continue
        match = PATTERN.fullmatch(line)
        if not match:
            return None
        r = dict(zip(FIELDS, map(int, match.groups()[1:])))
        if any(v >= 2**64 for v in r.values()):
            return None
        r['name'] = match[1]
        # Incomplete/straddling saves and screenshot/exit writes aren't idle.
        if r['start_us'] < start or r['end_us'] > end:
            continue
        if (r['name'] not in ('session-a.log', 'session-b.log') or
                r['end_us'] <= r['start_us'] or r['ok'] != 1 or r['valid'] != 1 or
                r['failures'] or not 0 < r['bytes'] <= 65536):
            return None
        elapsed = r['end_us'] - r['start_us']
        io = sum(r[k] for k in ('write_us', 'verify_us', 'sync_us'))
        if (io + r['prepare_us'] != elapsed or
                sum(r[k] for k in ('cbw_us', 'data_us', 'csw_us')) > io or
                any(r[p+'_us'] and not r[p+'_calls'] for p in ('cbw', 'data', 'csw'))):
            return None
        old = records.setdefault(r['start_us'], r)
        if old != r:
            return None
    ordered = sorted(records.values(), key=lambda r: r['start_us'])
    if not ordered or any(a['end_us'] > b['start_us'] for a, b in zip(ordered, ordered[1:])):
        return None
    totals = {k: sum(r[k] for r in ordered) for k in FIELDS
              if k not in ('start_us', 'end_us', 'ok', 'valid')}
    totals['elapsed_us'] = sum(r['end_us'] - r['start_us'] for r in ordered)
    return {'count': len(ordered), 'window_us': end-start, 'records': ordered,
            'totals': totals,
            'scope': 'Completed periodic saves wholly inside idle; wall time includes yielded tasks. '
                     'USB callbacks are system-wide during each save, not pure device wait or CPU runtime. '
                     'Only records persisted by a subsequent snapshot are available.'}
