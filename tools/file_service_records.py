"""Whole saved receipts for completed process-owned file services, never tail inference."""
import re

RECORD = re.compile(rb'file-service: stopped version=1 pid=(\d+) role=(\d+) requested=(\d+) opened=(\d+) failures=(\d+) abandoned=(\d+) retired=(\d+) retire_failed=(\d+) pending=(\d+) join_error=(\d+) max_start_us=(\d+) max_retire_us=(\d+) async=yes joined=yes')
FIELDS = ('pid','role','requested','opened','failures','abandoned','retired','retire_failed','pending','join_error','max_start_us','max_retire_us')

def file_service_receipts(snapshots, expected):
    rows = {}
    for raw in snapshots:
        seen = set()
        for line in raw.split(b'\n')[:-1]:
            line=line.removesuffix(b'\r')
            match = RECORD.fullmatch(line)
            if not match:
                continue
            values = tuple(map(int, match.groups()))
            row = dict(zip(FIELDS, values));pid=row['pid']
            assert pid > 0 and row['role'] in (1,2)
            assert pid not in seen, 'duplicate service receipt within a snapshot'
            seen.add(pid)
            assert pid not in rows or rows[pid] == row, 'conflicting service receipt'
            assert not any(row[key] for key in ('failures','abandoned','retire_failed','pending','join_error')), row
            assert row['requested'] == row['opened'] and row['retired'] > 0, row
            assert row['max_retire_us'] > 0 and (not row['opened'] or row['max_start_us'] > 0), row
            rows[pid] = row
    for pid,role in expected.items():
        assert pid in rows and rows[pid]['role'] == role, ('missing/wrong service receipt',pid,role)
    return [rows[pid] for pid in sorted(rows)]
