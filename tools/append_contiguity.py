"""Validate boot-local append-phase observations, never operation durability."""
import re

FIELDS = 'snapshot_us local_complete links_avoided prefix_bytes invalid'.split()
ROW = re.compile(rb'exfat-append-contiguous: version=1 ' +
                 b' '.join(k.encode() + rb'=(\d+)' for k in FIELDS))


def append_contiguity(blobs, *, require_exercised=False):
    rows = {}
    for blob in blobs:
        for line in blob.splitlines():
            if not line.startswith(b'exfat-append-contiguous:'):
                continue
            match = ROW.fullmatch(line)
            if not match:
                raise ValueError('Malformed contiguous append observation')
            row = dict(zip(FIELDS, map(int, match.groups())))
            if any(value >= 1 << 64 for value in row.values()) or not row['snapshot_us']:
                raise ValueError('Invalid contiguous append bounds')
            if row['invalid']:
                raise ValueError('Invalid contiguous append accounting')
            count = row['local_complete']
            if (not count and (row['links_avoided'] or row['prefix_bytes']) or
                    row['links_avoided'] < count or row['prefix_bytes'] // 512 < count):
                raise ValueError('Inconsistent contiguous append accounting')
            at = row['snapshot_us']
            if at in rows and rows[at] != row:
                raise ValueError('Conflicting contiguous append observations')
            rows[at] = row
    if not rows:
        raise ValueError('Missing contiguous append observation')
    previous = None
    for at in sorted(rows):
        row = rows[at]
        if previous and any(row[k] < previous[k] for k in FIELDS[1:-1]):
            raise ValueError('Regressing contiguous append counters')
        previous = row
    if require_exercised and not previous['local_complete']:
        raise ValueError('Contiguous append path not exercised')
    return dict(status='VALID', **previous, exercised=bool(previous['local_complete']),
                snapshots=len(rows),
                scope='Global boot-local append phases; outer mutation can still fail. '
                      'Not durable commits, per-file attribution or physical bytes saved.')
