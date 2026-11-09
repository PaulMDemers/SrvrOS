"""Strict fixed-record screenshot write observations; no inferred CPU time."""
import re
SITES=('call','gate','handler','handoff','reserve','copy')
def capture_write_timing(blobs,expected_records,expected_bytes,require_final=True):
    groups={}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith(('capture-write-snapshot:','capture-write-record:','capture-write-sample:')):continue
            d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
            if d.get('version')!=1 or 'snapshot_us' not in d:raise ValueError('Unsupported capture write record')
            g=groups.setdefault(d['snapshot_us'],{'records':{},'samples':{}})
            if line.startswith('capture-write-snapshot:'):key='header';old=g.get(key);g[key]=d
            elif line.startswith('capture-write-record:'):key=d['pid'];old=g['records'].get(key);g['records'][key]=d
            else:key=(d['pid'],d['site']);old=g['samples'].get(key);g['samples'][key]=d
            if old is not None and old!=d:raise ValueError('Conflicting capture write record')
    for at,g in sorted(groups.items(),reverse=True):
        h=g.get('header')
        if not h or (require_final and h.get('final')!=1):continue
        if h.get('enabled')!=1 or h.get('invalid')!=0 or h.get('dropped')!=0:raise ValueError('Invalid/dropped capture write evidence')
        rs=g['records'];ss=g['samples'];final=h['final']
        if len(rs)!=expected_records or len(ss)!=expected_records*len(SITES):continue
        output=[]
        for pid,r in sorted(rs.items()):
            if not pid or any(r.get(k)!=v for k,v in [('retired',1),('failures',0),('invalid',0),('final',final)]):raise ValueError('Incomplete/failed write lifetime')
            calls=r['calls']
            if calls<((expected_bytes+65535)//65536) or r['attempts']!=calls or r['bytes']!=expected_bytes or not expected_bytes<=r['requested']<=calls*65536:raise ValueError('Write call/byte evidence mismatch')
            if not 0<r['first_us']<=r['last_us'] or not 0<=r['growths']<=calls:raise ValueError('Invalid write clock/growth evidence')
            samples={}
            for site,name in enumerate(SITES):
                p=ss.get((pid,site))
                if not p or p.get('final')!=final or p.get('count')!=calls or not 0<=p.get('max_us',-1)<=p.get('total_us',-1):raise ValueError('Incomplete write phase evidence')
                samples[name]=p
            total=lambda name:samples[name]['total_us']
            if total('call')>r['last_us']-r['first_us'] or total('gate')+total('handler')+total('handoff')>total('call') or total('reserve')+total('copy')>total('handler'):raise ValueError('Inconsistent nested write intervals')
            output.append(dict(record=r,sites=samples))
        return dict(status='VALID',snapshot_us=at,final=final,workers=output,scope='Raw checked SYS_WRITE wall spans; reserve/copy nest in handler. POSIX wrapper/progress/user scheduling outside the raw call is unmeasured.')
    raise ValueError('Missing complete final capture write evidence')

def capture_gate_timing(blobs,write_timing):
    """Require exact companions for the already-selected final write snapshot."""
    at=write_timing['snapshot_us'];rows={};owners={}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith(('capture-write-gate:','capture-write-gate-owner:')):continue
            d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
            if d.get('version')!=1 or 'snapshot_us' not in d:raise ValueError('Unsupported gate timing record')
            if d['snapshot_us']!=at:continue
            if not {'pid','final'}<=d.keys():raise ValueError('Malformed gate identity')
            if line.startswith('capture-write-gate-owner:'):table=owners;key=d['pid']
            else:
                if d.get('kind') not in (0,1):raise ValueError('Malformed gate kind')
                table=rows;key=(d['pid'],d['kind'])
            if key in table and table[key]!=d:raise ValueError('Conflicting gate timing record')
            table[key]=d
    workers=write_timing['workers'];pids={w['record']['pid'] for w in workers}
    if set(owners)!=pids or set(rows)!={(pid,k) for pid in pids for k in (0,1)}:raise ValueError('Missing/extra gate companions')
    fields=('calls','wall_us','owned_us','scopes','blocked_us','ready_us','running_us','blocks','dispatches','exclusions','invalid')
    output=[]
    for w in workers:
        pid=w['record']['pid'];total=rows[(pid,0)];worst=rows[(pid,1)];owner=owners[pid]
        for c in (total,worst):
            if not set(fields)<=c.keys() or c.get('active')!=0 or c['invalid']!=0 or c['final']!=write_timing['final']:raise ValueError('Incomplete gate timing')
            if c['blocked_us']+c['ready_us']+c['running_us']!=c['wall_us'] or c['owned_us']>c['wall_us']:raise ValueError('Gate partition mismatch')
            if c['dispatches']<c['blocks'] or (c['blocked_us'] and not c['blocks']) or (c['ready_us'] and not c['dispatches']):raise ValueError('Gate scheduling mismatch')
        if total['calls']!=w['record']['calls'] or worst['calls']!=1 or any(worst[k]>total[k] for k in fields):raise ValueError('Gate count/summary mismatch')
        gate=w['sites']['gate']
        if total['wall_us']>gate['total_us'] or worst['wall_us']>gate['max_us']:raise ValueError('Gate outside raw write span')
        required={'start_us','end_us','thread','owner_pid','syscall','scope_start_us','scope_end_us','overlap_start_us','overlap_end_us'}
        if not required<=owner.keys() or owner['final']!=write_timing['final']:raise ValueError('Incomplete gate owner')
        if not w['record']['first_us']<=owner['start_us']<=owner['end_us']<=w['record']['last_us'] or owner['end_us']-owner['start_us']!=worst['wall_us']:raise ValueError('Invalid gate lifetime')
        if worst['scopes']:
            if not owner['thread'] or not 0<owner['scope_start_us']<=owner['overlap_start_us']<=owner['overlap_end_us']==owner['scope_end_us']:raise ValueError('Invalid owner scope')
            if not owner['start_us']<=owner['overlap_start_us']<=owner['overlap_end_us']<=owner['end_us'] or owner['overlap_end_us']-owner['overlap_start_us']>worst['owned_us']:raise ValueError('Invalid owner overlap')
        elif worst['owned_us'] or any(owner[k] for k in required-{'start_us','end_us'}):raise ValueError('Spurious gate owner')
        output.append(dict(pid=pid,total=total,worst=worst,owner=owner))
    return dict(status='VALID',snapshot_us=at,workers=output,scope='Complete gate wall partition and ownership overlap; worst gate retains its longest overlapping owner. Not CPU time or a foreign operation trace.')
