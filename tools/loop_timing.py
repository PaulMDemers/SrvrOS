"""Validate retained, whole-record desktop loop wall-time diagnostics."""
STAGES = ('control', 'messages', 'lifecycle', 'service', 'trial', 'input',
          'render', 'post', 'wait')
FIELDS = ('version', 'samples', 'over_limit', 'invalid', 'seq', 'start_us',
          'end_us', 'gap_us', 'cycle', 'pid', 'phase', 'end_cycle', 'end_pid',
          'end_phase', *(name+'_us' for name in STAGES), 'final')
PREFIX = b'desktop-loop-worst: '


def validate_loop_timing(snapshots, maximum=None, *, require_final=True):
    records = {}
    for snapshot in snapshots:
        for line in snapshot.splitlines():
            if not line.startswith(PREFIX):
                continue
            tokens = line[len(PREFIX):].split()
            assert len(tokens) == len(FIELDS), 'incomplete loop receipt'
            row = {}
            for field, token in zip(FIELDS, tokens):
                key, separator, value = token.partition(b'=')
                assert key == field.encode() and separator and value.isdigit(), 'invalid loop field'
                row[field] = int(value)
                assert row[field] <= 2**64-1, 'loop field overflow'
            assert row['version'] == 1 and row['invalid'] == 0, 'invalid loop measurement'
            assert 0 < row['seq'] <= row['samples'], 'loop sequence'
            assert 0 < row['start_us'] < row['end_us'], 'loop clock'
            assert row['gap_us'] == row['end_us']-row['start_us'], 'loop duration'
            assert sum(row[name+'_us'] for name in STAGES) == row['gap_us'], 'loop stage conservation'
            assert 0 <= row['over_limit'] <= row['samples'], 'loop count'
            assert bool(row['over_limit']) == (row['gap_us'] > 500000), 'loop bound/count'
            assert row['phase'] <= 3 and row['end_phase'] <= 3 and row['final'] <= 1, 'loop phase'
            records[tuple(row.values())] = row
    rows = sorted(records.values(), key=lambda row: (row['samples'], row['final']))
    assert rows, 'missing retained loop receipt'
    previous = None
    identity = [field for field in FIELDS if field not in ('samples', 'over_limit', 'final')]
    for row in rows:
        if previous:
            assert row['seq'] >= previous['seq'] and row['gap_us'] >= previous['gap_us'], 'lost loop maximum'
            assert row['over_limit'] >= previous['over_limit'], 'lost loop count'
            if row['seq'] == previous['seq']:
                assert all(row[field] == previous[field] for field in identity), 'changed retained interval'
        previous = row
    final = [row for row in rows if row['final']]
    assert len(final) <= 1 and (not final or final[0] == rows[-1]), 'conflicting final loop receipt'
    if require_final:
        assert len(final) == 1, 'missing final loop receipt'
    result = final[0] if final else rows[-1]
    if maximum is not None:
        assert result['gap_us'] == maximum, 'loop maximum differs from lifecycle receipt'
    return result
