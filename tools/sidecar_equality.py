"""Strict boot-lifetime exact-media sidecar proof counters, not saved-write bytes."""
import re
def sidecar_equality(blobs):
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
    if not r['snapshot_us'] or not r['checks'] or not r['equal'] or not r['retained_bytes'] or any(r[k] for k in ('refused','flush_failed','active','invalid')):raise ValueError('Unexercised/incomplete/failed sidecar equality')
    if r['checks']!=r['equal']+r['changed']+r['refused'] or r['retained_bytes']<r['equal']:raise ValueError('Inconsistent sidecar counters')
    return dict(status='VALID',**r)
