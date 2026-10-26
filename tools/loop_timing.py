"""Validate retained, whole-record desktop loop wall-time diagnostics."""
STAGES = ('control', 'messages', 'lifecycle', 'service', 'trial', 'input',
          'render', 'post', 'wait')
FIELDS = ('version', 'samples', 'over_limit', 'invalid', 'seq', 'start_us',
          'end_us', 'gap_us', 'cycle', 'pid', 'phase', 'end_cycle', 'end_pid',
          'end_phase', *(name+'_us' for name in STAGES), 'final')
PREFIX = b'desktop-loop-worst: '


def validate_loop_timing(snapshots, maximum=None, *, require_final=True):
    snapshots = tuple(snapshots)
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
            assert row['version'] in (1, 2) and row['invalid'] == 0, 'invalid loop measurement'
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
    if result['version'] == 2:
        wait, sched = validate_wait_timing(snapshots, result)
        result = dict(result, wait=wait, scheduler=sched)
    return result

WAIT_FIELDS = ('seq', 'valid', 'begin_us', 'gui_begin_us', 'gui_end_us',
               'sleep_begin_us', 'sleep_end_us', 'end_us', 'gui_result', 'sleep_result', 'final')
SCHED_FIELDS = ('seq', 'version', 'start_us', 'end_us', 'start_tick', 'end_tick',
                'blocked_us', 'ready_us', 'blocks', 'signals', 'timeouts', 'dispatches',
                'exclusions', 'invalid', 'max_ready_us', 'wake_us', 'run_us', 'wake_tick',
                'deadline_tick', 'reason', 'ready_exclusions', 'late_ticks', 'final')


def validate_wait_timing(snapshots, worst):
    selected = []
    for prefix, fields in ((b'desktop-loop-wait: ', WAIT_FIELDS),
                            (b'desktop-loop-sched: ', SCHED_FIELDS)):
        rows = {}
        for snapshot in snapshots:
            for line in snapshot.splitlines():
                if not line.startswith(prefix):
                    continue
                tokens = line[len(prefix):].split()
                assert len(tokens) == len(fields), 'incomplete wait/scheduler receipt'
                row = {}
                for field, token in zip(fields, tokens):
                    key, separator, value = token.partition(b'=')
                    signed = field in ('gui_result', 'sleep_result')
                    assert key == field.encode() and separator and (value.isdigit() or
                        (signed and value.startswith(b'-') and value[1:].isdigit())), 'invalid wait field'
                    row[field] = int(value)
                    assert (-(2**63) if signed else 0) <= row[field] <= (2**63-1 if signed else 2**64-1)
                assert row['seq'] > 0 and row['final'] in (0, 1)
                key = (row['seq'], row['final'])
                assert key not in rows or rows[key] == row, 'changed retained wait/scheduler interval'
                rows[key] = row
        key = (worst['seq'], worst['final'])
        assert key in rows, 'missing matching wait/scheduler receipt'
        row = rows[key]
        other = rows.get((worst['seq'], 1-worst['final']))
        assert not other or all(row[f] == other[f] for f in fields if f != 'final'), 'changed final wait detail'
        selected.append(row)
    w, q = selected
    assert w['valid'] in (0, 1)
    if not w['valid']:
        assert not worst['wait_us'], 'missing wait instrumentation'
        assert all(not w[f] for f in WAIT_FIELDS if f not in ('seq', 'final'))
        assert all(not q[f] for f in SCHED_FIELDS if f not in ('seq', 'final'))
        return w, q
    assert worst['start_us'] <= w['begin_us'] <= w['end_us'] <= worst['end_us']
    assert w['end_us']-w['begin_us'] <= worst['wait_us']
    assert q['version'] == 1 and q['invalid'] == 0
    assert w['begin_us'] <= q['start_us'] <= q['end_us'] <= w['end_us']
    assert q['start_tick'] <= q['end_tick']
    assert q['blocked_us']+q['ready_us'] <= q['end_us']-q['start_us'], 'scheduler state time exceeds interval'
    assert q['signals']+q['timeouts'] == q['blocks'] <= q['dispatches']
    assert q['max_ready_us'] <= q['ready_us'] and q['ready_exclusions'] <= q['exclusions']
    assert q['reason'] <= 2
    if q['max_ready_us']:
        assert q['start_us'] <= q['wake_us'] <= q['run_us'] <= q['end_us']
        assert q['run_us']-q['wake_us'] == q['max_ready_us']
        assert q['start_tick'] <= q['wake_tick'] <= q['end_tick']
        if q['reason'] == 2:
            assert 0 < q['deadline_tick'] <= q['wake_tick']
    else:
        assert not any(q[f] for f in ('wake_us', 'run_us', 'wake_tick', 'deadline_tick', 'reason', 'ready_exclusions'))
    cursor = q['start_us']
    assert w['gui_begin_us'] or w['sleep_begin_us'], 'no observed wait call'
    for name in ('gui', 'sleep'):
        begin, end = w[name+'_begin_us'], w[name+'_end_us']
        if begin:
            assert cursor <= begin <= end <= q['end_us'], 'wait call order'
            cursor = end
        else:
            assert end == w[name+'_result'] == 0
    if w['gui_begin_us']:
        assert bool(w['sleep_begin_us']) == (w['gui_result'] < 0), 'fallback differs from wait result'
    return w, q
