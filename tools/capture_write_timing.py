"""Strict fixed-record screenshot write observations; no inferred CPU time."""
import re
SITES=('call','gate','handler','handoff','reserve','copy')
FAST_FIELDS=('fast_calls','fast_requested','fast_bytes','fast_failures','fast_max_request','fast_max_bytes')
_V2_FIELDS={
 'snapshot':'version snapshot_us enabled invalid dropped final',
 'record':'version snapshot_us pid first_us last_us retired attempts calls requested bytes failures growths invalid fast_calls fast_requested fast_bytes fast_failures fast_max_request fast_max_bytes final',
 'sample':'version snapshot_us pid site count total_us max_us final',
}

def capture_write_timing(blobs,expected_records,expected_bytes,require_final=True):
    groups={}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if not line.startswith(('capture-write-snapshot:','capture-write-record:','capture-write-sample:')):continue
            d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
            version=d.get('version')
            if version not in (1,2) or 'snapshot_us' not in d:raise ValueError('Unsupported capture write record')
            kind=line.partition(':')[0].removeprefix('capture-write-')
            if version==2:
                fields=_V2_FIELDS[kind].split()
                if not re.fullmatch('capture-write-'+kind+': '+' '.join(k+r'=(\d+)' for k in fields),line) or any(v>=2**64 for v in d.values()):raise ValueError('Malformed v2 capture write record')
            elif any(k in d for k in FAST_FIELDS):raise ValueError('Unversioned private-copy partition')
            g=groups.setdefault(d['snapshot_us'],{'records':{},'samples':{},'version':version})
            if g['version']!=version:raise ValueError('Mixed capture write versions')
            if kind=='snapshot':key='header';old=g.get(key);g[key]=d
            elif kind=='record':key=d['pid'];old=g['records'].get(key);g['records'][key]=d
            else:key=(d['pid'],d['site']);old=g['samples'].get(key);g['samples'][key]=d
            if old is not None and old!=d:raise ValueError('Conflicting capture write record')
    for at,g in sorted(groups.items(),reverse=True):
        h=g.get('header')
        if not h or (require_final and h.get('final')!=1):continue
        if h.get('enabled')!=1 or h.get('invalid')!=0 or h.get('dropped')!=0:raise ValueError('Invalid/dropped capture write evidence')
        rs=g['records'];ss=g['samples'];final=h['final'];version=g['version']
        if len(rs)!=expected_records or len(ss)!=expected_records*len(SITES):continue
        output=[]
        for pid,r in sorted(rs.items()):
            if not pid or any(r.get(k)!=v for k,v in [('retired',1),('failures',0),('invalid',0),('final',final)]):raise ValueError('Incomplete/failed write lifetime')
            calls=r['calls']
            if calls<((expected_bytes+65535)//65536) or r['attempts']!=calls or r['bytes']!=expected_bytes or not expected_bytes<=r['requested']<=calls*65536:raise ValueError('Write call/byte evidence mismatch')
            if not 0<r['first_us']<=r['last_us'] or not 0<=r['growths']<=calls:raise ValueError('Invalid write clock/growth evidence')
            ordinary=calls
            if version==2:
                fast=r['fast_calls'];ordinary=calls-fast
                if fast>calls or r['fast_failures'] or r['growths']>ordinary:raise ValueError('Invalid private-copy partition')
                if not fast:
                    if any(r[k] for k in FAST_FIELDS):raise ValueError('Private evidence without private calls')
                elif not (fast<=r['fast_bytes']<=r['fast_requested']<=fast*65536 and
                          0<r['fast_max_bytes']<=r['fast_max_request']<=65536 and
                          r['fast_max_bytes']<=r['fast_bytes']<=fast*r['fast_max_bytes'] and
                          r['fast_max_request']<=r['fast_requested']<=fast*r['fast_max_request']):raise ValueError('Unbounded/inconsistent private copy evidence')
                remaining_bytes=r['bytes']-r['fast_bytes'];remaining_requested=r['requested']-r['fast_requested']
                if not (ordinary<=remaining_bytes<=remaining_requested<=ordinary*65536):raise ValueError('Unbound ordinary byte partition')
            samples={}
            for site,name in enumerate(SITES):
                p=ss.get((pid,site));count=ordinary if version==2 and name in ('gate','handoff') else calls
                if not p or p.get('final')!=final or p.get('count')!=count or not 0<=p.get('max_us',-1)<=p.get('total_us',-1):raise ValueError('Incomplete write phase evidence')
                if not count and (p['total_us'] or p['max_us']):raise ValueError('Spurious ordinary phase time')
                samples[name]=p
            total=lambda name:samples[name]['total_us']
            if total('call')>r['last_us']-r['first_us'] or total('gate')+total('handler')+total('handoff')>total('call') or total('reserve')+total('copy')>total('handler'):raise ValueError('Inconsistent nested write intervals')
            item=dict(record=r,sites=samples)
            if version==2:item['ordinary_calls']=ordinary
            output.append(item)
        result=dict(status='VALID',snapshot_us=at,final=final,workers=output,scope='Raw checked SYS_WRITE wall spans; reserve/copy nest in handler. POSIX wrapper/progress/user scheduling outside the raw call is unmeasured.')
        if version==2:
            result['version']=2
            result['scope']='Raw SYS_WRITE partition: ordinary storage gates/handoffs and opt-in atomic reserved copies. Capacity proof/copy nest in handler; no synthetic owner-free gate or CPU/hardware timing.'
        return result
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
        ordinary=w.get('ordinary_calls',w['record']['calls'])
        if total['calls']!=ordinary or worst['calls']!=(1 if ordinary else 0) or any(worst[k]>total[k] for k in fields):raise ValueError('Gate count/summary mismatch')
        gate=w['sites']['gate']
        if total['wall_us']>gate['total_us'] or worst['wall_us']>gate['max_us']:raise ValueError('Gate outside raw write span')
        required={'start_us','end_us','thread','owner_pid','syscall','scope_start_us','scope_end_us','overlap_start_us','overlap_end_us'}
        if not required<=owner.keys() or owner['final']!=write_timing['final']:raise ValueError('Incomplete gate owner')
        if not ordinary:
            if any(total[k] or worst[k] for k in fields) or any(owner[k] for k in required):raise ValueError('Spurious ordinary gate for private-only writes')
        elif not w['record']['first_us']<=owner['start_us']<=owner['end_us']<=w['record']['last_us'] or owner['end_us']-owner['start_us']!=worst['wall_us']:raise ValueError('Invalid gate lifetime')
        if worst['scopes']:
            if not owner['thread'] or not 0<owner['scope_start_us']<=owner['overlap_start_us']<=owner['overlap_end_us']==owner['scope_end_us']:raise ValueError('Invalid owner scope')
            if not owner['start_us']<=owner['overlap_start_us']<=owner['overlap_end_us']<=owner['end_us'] or owner['overlap_end_us']-owner['overlap_start_us']>worst['owned_us']:raise ValueError('Invalid owner overlap')
        elif worst['owned_us'] or any(owner[k] for k in required-{'start_us','end_us'}):raise ValueError('Spurious gate owner')
        output.append(dict(pid=pid,total=total,worst=worst,owner=owner))
    return dict(status='VALID',snapshot_us=at,workers=output,scope='Complete gate wall partition and ownership overlap; worst gate retains its longest overlapping owner. Not CPU time or a foreign operation trace.')
