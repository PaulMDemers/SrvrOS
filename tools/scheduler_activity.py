"""Strict CPU0 worker attribution from diagnostic snapshots, never energy."""
import re

_U = r'([0-9]{1,20})'
_HEADER = re.compile(r'worker-snapshot: us=' + _U + r' count=' + _U + r' version=1')
_END = re.compile(r'worker-snapshot-end: us=' + _U + r' count=' + _U)
_ROW = re.compile(r'worker: us=' + _U + r' slot=' + _U + r' id=' + _U +
                  r' name=([^\s]{1,31}) state=' + _U + r' ticks=' + _U +
                  r' switches=' + _U + r' timeouts=' + _U + r' signals=' + _U)
_COUNTERS = ('ticks', 'switches', 'timeouts', 'signals')


def worker_activity(text, start_us, end_us):
    """Return covered stable lifetimes only; missing/malformed evidence is None.

    Snapshots can be interleaved with unrelated console records. No inference is
    made for workers born/retired between endpoints; list those identities apart.
    """
    if not 0 <= start_us < end_us < 2**64:
        return None
    blocks = []
    active = None
    for line in text.splitlines():
        if not line.startswith(('worker:', 'worker-snapshot:', 'worker-snapshot-end:')):
            continue
        header = _HEADER.fullmatch(line)
        if header:
            at, count = map(int, header.groups())
            if active is not None or not 0 < at < 2**64 or not 0 <= count <= 32:
                return None
            active = (at, count, {})
            continue
        row = _ROW.fullmatch(line)
        if row:
            if active is None:
                return None
            at, slot, identity, name, state, *counts = row.groups()
            at, slot, identity, state = map(int, (at, slot, identity, state))
            counts = list(map(int, counts))
            if at != active[0] or not 0 <= slot < 32 or not 0 < identity < 2**64 or \
               not 1 <= state <= 5 or any(n >= 2**64 for n in counts) or \
               identity in active[2] or any(r['slot'] == slot for r in active[2].values()):
                return None
            active[2][identity] = dict(slot=slot, name=name, state=state, **dict(zip(_COUNTERS, counts)))
            continue
        end = _END.fullmatch(line)
        if end:
            if active is None or tuple(map(int, end.groups())) != active[:2] or len(active[2]) != active[1]:
                return None
            if blocks and active[0] <= blocks[-1][0]:
                return None
            blocks.append((active[0], active[2]))
            active = None
            continue
        return None
    if active is not None:
        return None
    blocks = [(at, rows) for at, rows in blocks if start_us <= at <= end_us]
    if len(blocks) < 3 or blocks[-1][0] - blocks[0][0] < 20_000_000:
        return None
    if any(b[0] - a[0] > 20_000_000 for a, b in zip(blocks, blocks[1:])):
        return None
    identities = set().union(*(set(rows) for _, rows in blocks))
    stable = set.intersection(*(set(rows) for _, rows in blocks))
    # Even partial lifetimes must not regress or change identity/name bindings.
    seen = {}
    for _, rows in blocks:
        for identity, row in rows.items():
            old = seen.get(identity)
            if old and (row['slot'] != old['slot'] or row['name'] != old['name'] or
                        any(row[k] < old[k] for k in _COUNTERS)):
                return None
            seen[identity] = row
    covered = blocks[-1][0] - blocks[0][0]
    workers = []
    for identity in sorted(stable):
        first, last = blocks[0][1][identity], blocks[-1][1][identity]
        delta = {k: last[k] - first[k] for k in _COUNTERS}
        workers.append(dict(identity=identity, slot=last['slot'], name=last['name'], **delta,
                            timeout_wakes_per_second=round(delta['timeouts'] * 1_000_000 / covered, 3),
                            signal_wakes_per_second=round(delta['signals'] * 1_000_000 / covered, 3)))
    workers.sort(key=lambda row: (-row['switches'], row['identity']))
    return dict(start_us=blocks[0][0], end_us=blocks[-1][0], covered_us=covered,
                checkpoints=len(blocks), workers=workers,
                partial_lifetime_ids=sorted(identities - stable),
                scope='CPU0 thread transitions and timer samples; not hardware wakeups, runtime, or energy')
