"""Strict companion attribution for the same retained worst desktop loop.

Older loop receipts remain readable by loop_timing; they cannot satisfy this
new proof. No UART reconstruction or joining partial lines is permitted.
"""
from loop_timing import STAGES, SCHED_FIELDS, validate_loop_timing

RENDER_FIELDS = ('seq','version','valid','begin_us','present_begin_us','fence_begin_us',
                 'finish_begin_us','end_us','damage','scale','capture_id','capture_busy',
                 'compose_us','upload_us','compose_calls','upload_calls','compose_pixels','upload_pixels','final')
CAPTURE_FIELDS = ('seq','version','valid','begin_us','end_us','id_before','id_after',
                  'busy_before','busy_after','pending_before','pending_after','final')


def _selected(snapshots, prefix, fields, worst):
    records = {}
    for raw in snapshots:
        for line in raw.splitlines(keepends=True):
            if not line.startswith(prefix):continue
            assert line.endswith(b'\n'), 'truncated attribution record'
            words = line[len(prefix):].split()
            assert len(words) == len(fields), 'attribution field count'
            row = {}
            for field, word in zip(fields, words):
                key, sep, value = word.partition(b'=')
                assert sep and key == field.encode() and value.isdigit(), 'attribution field order/type'
                row[field] = int(value)
                assert row[field] < 1 << 64, 'attribution overflow'
            assert row['final'] in (0,1), 'attribution final flag'
            key = row['seq'], row['final']
            assert key not in records or records[key] == row, 'conflicting attribution'
            records[key] = row
    matches = [q for q in records.values() if q['seq']==worst['seq']]
    assert matches, 'missing worst-loop attribution'
    identity = fields[:-1]
    assert all(all(q[f]==matches[0][f] for f in identity) for q in matches), 'changed retained attribution'
    key = worst['seq'],worst['final']
    assert key in records, 'missing corresponding final attribution'
    return records[key]


def _scheduler(q, begin, end):
    assert q['version']==1 and q['invalid']==0
    assert begin <= q['start_us'] <= q['end_us'] <= end
    assert q['start_tick'] <= q['end_tick']
    assert q['blocked_us']+q['ready_us'] <= q['end_us']-q['start_us']
    assert q['signals']+q['timeouts']==q['blocks']<=q['dispatches']
    assert q['max_ready_us']<=q['ready_us'] and q['ready_exclusions']<=q['exclusions']
    assert q['reason']<=2
    if q['max_ready_us']:
        assert q['start_us']<=q['wake_us']<q['run_us']<=q['end_us']
        assert q['run_us']-q['wake_us']==q['max_ready_us']
        assert q['start_tick']<=q['wake_tick']<=q['end_tick']
        if q['reason']==2:assert 0<q['deadline_tick']<=q['wake_tick']
    else:
        assert not any(q[f] for f in ('wake_us','run_us','wake_tick','deadline_tick','reason','ready_exclusions'))


def validate_render_timing(snapshots, maximum=None, *, require_final=True):
    snapshots=tuple(snapshots)
    worst=validate_loop_timing(snapshots,maximum,require_final=require_final)
    r=_selected(snapshots,b'desktop-loop-render: ',RENDER_FIELDS,worst)
    c=_selected(snapshots,b'desktop-loop-capture: ',CAPTURE_FIELDS,worst)
    rs=_selected(snapshots,b'desktop-loop-render-sched: ',SCHED_FIELDS,worst)
    cs=_selected(snapshots,b'desktop-loop-capture-sched: ',SCHED_FIELDS,worst)
    render_start=worst['start_us']+sum(worst[f+'_us'] for f in STAGES[:6])
    render_end=render_start+worst['render_us'];post_end=render_end+worst['post_us']
    for row,q,start,end in ((r,rs,render_start,render_end),(c,cs,render_end,post_end)):
        assert row['version']==1 and row['valid'] in (0,1)
        if not row['valid']:
            assert start==end, 'missing measured stage'
            assert not any(v for k,v in row.items() if k not in ('seq','version','final'))
            assert not any(v for k,v in q.items() if k not in ('seq','final'))
            continue
        assert start<=row['begin_us']<=row['end_us']<=end
        _scheduler(q,row['begin_us'],row['end_us'])
    if r['valid']:
        times=[r[k] for k in ('begin_us','present_begin_us','fence_begin_us','finish_begin_us','end_us')]
        assert times==sorted(times) and times[0]>0
        assert r['scale'] in (1,2) and r['damage']<=16 and r['capture_busy'] in (0,1)
        assert r['compose_calls']==r['upload_calls']==r['damage']
        assert r['compose_us']+r['upload_us']<=times[2]-times[1]
        if not r['damage']:assert not any(r[k] for k in ('compose_us','upload_us','compose_pixels','upload_pixels'))
    if c['valid']:
        assert all(c[k] in (0,1) for k in ('busy_before','busy_after','pending_before','pending_after'))
        assert c['id_before']<=c['id_after']<=c['id_before']+1
        if r['valid']:assert r['end_us']<=c['begin_us'] and r['capture_id']==c['id_before'] and r['capture_busy']==c['busy_before']
    phases={} if not r['valid'] else dict(stack_us=r['present_begin_us']-r['begin_us'],
        present_us=r['fence_begin_us']-r['present_begin_us'],fence_us=r['finish_begin_us']-r['fence_begin_us'],
        finish_us=r['end_us']-r['finish_begin_us'])
    return dict(sequence=worst['seq'],render=r,render_scheduler=rs,capture=c,capture_scheduler=cs,phases=phases)
