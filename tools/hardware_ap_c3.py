"""Coordinated C3 receipts: protocol success is distinct from observed residency."""
import re
from hardware_ap_owner import ap_owner_result
from hardware_cpu_load import mwait_result

def _result(text):
    heads=[s for s in text.splitlines() if s.startswith('ap-c3:')]
    if len(heads)!=1:return None
    m=re.fullmatch(r'ap-c3: v=1 mode=(native|vm-simulation) epoch=([0-9]+) rounds=32 fault=0 config=([0-9]+) limit_us=20000 completed=yes retained=yes',heads[0])
    if not m:return None
    mode,epoch,config=m.groups();epoch,config=int(epoch),int(config)
    native=mode=='native'
    if not 0<epoch<2**64 or not 0<=config<2**64 or (native and not 2<=config&15<=8) or (not native and config):return None
    owner_lines=[s for s in text.splitlines() if s.startswith('ap-owner-cpu:')]
    owner_header='ap-owner: v=1 admitted=1 mode='+('native' if native else 'vm')+' cpus=4 bsp=0 mask=15 method='+('monitor-c1' if native else 'pause')+' rounds=32 limit_us=20000 retained=yes'
    tails=[s for s in text.splitlines() if s.startswith('ap-owner: completed=')]
    if len(tails)!=1:return None
    original_heads=[s for s in text.splitlines() if s.startswith('ap-owner: v=')]
    if len(original_heads)>1:return None
    owner=ap_owner_result('\n'.join([*(original_heads or [owner_header]),*owner_lines,*tails]))
    if not owner or owner['mode']!=('native' if native else 'vm') or owner['cpus']!=4:return None
    peer_lines=[s for s in text.splitlines() if s.startswith('ap-c3-peer:')];peers={}
    for line in peer_lines:
        m=re.fullmatch(r'ap-c3-peer: id=([123]) mode=([12]) armed=32 done=32 attempts=32 early=0 config=([0-9]+),([0-9]+)',line)
        if not m:return None
        cpu,pm,a,b=map(int,m.groups())
        if cpu in peers or pm!=(1 if native else 2) or a!=config or b!=config:return None
        peers[cpu]=True
    if len(peer_lines)!=3 or set(peers)!={1,2,3}:return None
    lines=[s for s in text.splitlines() if s.startswith('ap-c3-round:')];rows=[];order=None
    for seq,line in enumerate(lines,1):
        m=re.fullmatch(r'ap-c3-round: seq=([0-9]+) us=([0-9]+) tsc=([0-9]+) core=([0-9]+) package=([0-9]+) p0=([0-9]+),([0-9]+),([0-9]+) p1=([0-9]+),([0-9]+),([0-9]+) p2=([0-9]+),([0-9]+),([0-9]+)',line)
        if not m:return None
        v=list(map(int,m.groups()))
        if any(x>=2**64 for x in v):return None
        n,us,tsc,core,pkg=v[:5];p=[v[5:8],v[8:11],v[11:14]];ids=[x[0] for x in p]
        if n!=seq or not 0<=us<=20000 or core>tsc or pkg>tsc or set(ids)!={1,2,3} or (order is not None and ids!=order):return None
        if any(c>t or (native and not t) for _,t,c in p) or (native and not tsc):return None
        if not native and (tsc or core or pkg or any(t or c for _,t,c in p)):return None
        order=ids;rows.append(dict(seq=seq,us=us,tsc=tsc,core=core,package=pkg,peers=p))
    if len(rows)!=32:return None
    sums={k:sum(r[k] for r in rows) for k in ('tsc','core','package')}
    if any(x>=2**64 for x in sums.values()):return None
    if native:
        mw=mwait_result(text)
        if not mw or mw['status']!='PASS' or mw['hint']!=16 or mw['epoch']!=epoch:return None
        d=mw['residency']
        if (d['tsc'],d['core_c3'],d['package_c3'])!=(sums['tsc'],sums['core'],sums['package']):return None
        audit=[s for s in text.splitlines() if s.startswith('cpu-idle-audit:')]
        if len(audit)!=1 or f'e2_start={config} e2_end={config}' not in audit[0]:return None
    return dict(status='PASS',mode=mode,epoch=epoch,rounds=32,max_round_us=max(x['us'] for x in rows),
                bsp_totals=sums,bsp_core_c3_observed=bool(native and sums['core']),
                package_c3_observed=bool(native and sums['package']),
                peer_core_c3_observed={str(i):bool(native and any(c for row in rows for cpu,t,c in row['peers'] if cpu==i)) for i in (1,2,3)},
                ownership=owner,rows=rows,scope='bounded coordination; residency zero is valid but unobserved; no energy or SMP scheduling acceptance')

def ap_c3_result(text):
    try:return _result(text)
    except (ValueError,KeyError,OverflowError):return None
