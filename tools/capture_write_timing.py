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
