"""Strict boot-lifetime exact-media sidecar proof counters, not saved-write bytes."""
import re
def sidecar_equality(blobs,require_differences=False,require_equal=True):
    fields='snapshot_us checks equal changed refused retained_bytes flush_failed active invalid'.split();rows={}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('exfat-sidecar:'):continue
            m=re.fullmatch('exfat-sidecar: version=1 '+' '.join(k+r'=(\d+)' for k in fields),line)
            if not m:raise ValueError('Malformed sidecar equality record')
            r=dict(zip(fields,map(int,m.groups())))
            if any(v>=2**64 for v in r.values()):raise ValueError('Overflowing sidecar equality record')
            at=r['snapshot_us']
            if at in rows and rows[at]!=r:raise ValueError('Conflicting sidecar equality record')
            rows[at]=r
    if not rows:raise ValueError('Missing sidecar equality evidence')
    r=rows[max(rows)]
    if not r['snapshot_us'] or not r['checks'] or (require_equal and (not r['equal'] or not r['retained_bytes'])) or any(r[k] for k in ('refused','flush_failed','active','invalid')):raise ValueError('Unexercised/incomplete/failed sidecar equality')
    if r['checks']!=r['equal']+r['changed']+r['refused'] or r['retained_bytes']<r['equal']:raise ValueError('Inconsistent sidecar counters')
    result=dict(status='VALID',**r)
    names='snapshot_us shape path inode mode nlink uid gid atime mtime ctime format last_offset'.split()
    differences={}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith('exfat-sidecar-diff:'):continue
            m=re.fullmatch('exfat-sidecar-diff: version=1 '+' '.join(k+r'=(\d+)' for k in names),line)
            if not m:raise ValueError('Malformed sidecar difference record')
            d=dict(zip(names,map(int,m.groups())))
            if any(v>=2**64 for v in d.values()):raise ValueError('Overflowing sidecar difference record')
            t=d['snapshot_us']
            if t in differences and differences[t]!=d:raise ValueError('Conflicting sidecar difference record')
            if t not in rows:raise ValueError('Unpaired sidecar difference record')
            if sum(d[k] for k in names[1:-1])!=rows[t]['changed']:raise ValueError('Inconsistent sidecar difference counters')
            differences[t]=d
    if require_differences and r['snapshot_us'] not in differences:raise ValueError('Missing final sidecar difference record')
    if r['snapshot_us'] in differences:result['differences']=differences[r['snapshot_us']]
    return result


def sidecar_equality_window(blobs,start,end):
    """Original two-completed-live-sample requirement, including equality growth.

    Re-emitted immutable snapshots retain their original sample clock; a final
    cumulative counter or a post-playback sample never supplies a live endpoint.
    """
    if not __debug__:raise RuntimeError('evidence validation requires Python assertions')
    assert 0<start<end,'Invalid live PCM interval'
    final=sidecar_equality(blobs);rows={}
    for blob in blobs:
        for line in blob.splitlines():
            if not line.startswith(b'exfat-sidecar:'):continue
            try:r=sidecar_equality([line])
            except ValueError:continue
            at=r['snapshot_us']
            if start<=at<=end:rows[at]=r
    assert len(rows)>=2,'Need two complete equality samples during live PCM'
    first,last=rows[min(rows)],rows[max(rows)]
    assert last['equal']>first['equal'],'No equality growth during live PCM'
    assert final['snapshot_us']>=end,'No completed post-playback sidecar evidence'
    return dict(status='PASS',pcm_start_us=start,pcm_end_us=end,first=first,last=last,
                equal_delta=last['equal']-first['equal'],final=final)
