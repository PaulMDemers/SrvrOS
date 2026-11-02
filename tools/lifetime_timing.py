"""Retained worst launch/Close observations; absent stages are not zero cost."""
import re
FIELDS=('version','snapshot_us','kind','pid','cycle','begin_us','launch_begin_us','launch_end_us','ready_us','close_request_us','visual_us','exit_seen_us','reap_queued_us','reap_begin_us','reap_end_us','closed_us','final')
def lifetime_worst_records(snapshots,ready_max,close_max):
    rows={}
    for raw in snapshots:
        for line in raw.splitlines(keepends=True):
            if not line.endswith(b'\n'):continue
            if not line.startswith(b'desktop-life-worst: '):continue
            words=line.split()[1:]
            if len(words)!=len(FIELDS):continue
            q={}
            for field,word in zip(FIELDS,words):
                k,sep,v=word.partition(b'=')
                assert k==field.encode() and sep,'invalid lifetime field'
                if field=='kind':assert v in (b'ready',b'close');q[field]=v.decode()
                else:assert v.isdigit();q[field]=int(v);assert q[field]<=2**64-1
            assert q['version']==1 and q['pid']>0 and q['final'] in (0,1)
            assert 0<q['begin_us']<=q['ready_us']<=q['snapshot_us']
            assert q['begin_us']<=q['launch_begin_us']<=q['launch_end_us']<=q['ready_us']
            for field in ('close_request_us','visual_us','exit_seen_us','reap_queued_us','reap_begin_us','reap_end_us','closed_us'):
                assert not q[field] or q['ready_us']<=q[field]<=q['snapshot_us']
            if q['closed_us']:assert q['close_request_us'] and q['close_request_us']<=q['closed_us']
            for field in ('visual_us','exit_seen_us'):
                if q[field]:assert q['close_request_us'] and q['close_request_us']<=q[field]
            if any(q[f] for f in ('reap_queued_us','reap_begin_us','reap_end_us')):
                assert q['close_request_us']<=q['reap_queued_us']<=q['reap_begin_us']<=q['reap_end_us']<=q['closed_us']
            key=q['snapshot_us'],q['kind'],q['final']
            assert key not in rows or rows[key]==q,'conflicting lifetime snapshot'
            rows[key]=q
    result={}
    for kind,maximum in [('ready',ready_max),('close',close_max)]:
        matches=[q for q in rows.values() if q['kind']==kind and q['final']==1]
        assert matches,'missing final worst lifetime'
        q=max(matches,key=lambda q:q['snapshot_us'])
        elapsed=q['ready_us']-q['begin_us'] if kind=='ready' else q['closed_us']-q['close_request_us']
        assert elapsed==maximum and elapsed>0,'worst lifetime differs from aggregate'
        result[kind]=q
    return result
