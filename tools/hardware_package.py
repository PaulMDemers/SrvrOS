"""Read-only package context; observations do not identify a causal blocker."""
import re
from hardware_ap_c3 import ap_c3_result

def irtl_decode(raw):
    unit=(raw>>10)&7
    if raw&~0x9fff or unit>5 or not raw&(1<<15):return None
    return {'valid':True,'unit_encoding':unit,'limit_ns':(raw&1023)*(1<<(unit*5))}

def package_result(text):
    try:return _parse(text)
    except (ValueError,KeyError,OverflowError):return None

def _parse(text):
    heads=[x for x in text.splitlines() if x.startswith('ap-package:')]
    if len(heads)!=1:return None
    m=re.fullmatch(r'ap-package: v=1 epoch=(\d+) rounds=32 fault=0 irtl=(\d+),(\d+) snap_us=(\d+),(\d+) devices=(\d+) policy_writes=0 gpu_rc6=unknown platform_permission=unknown completed=yes',heads[0])
    if not m:return None
    epoch,a,b,start_us,end_us,count=map(int,m.groups())
    if not 0<epoch<2**64 or not 0<=a==b<2**64 or not 0<start_us<end_us<2**64 or not 1<=count<=128:return None
    coord=ap_c3_result(text)
    if not coord or coord['mode']!='native' or coord['epoch']!=epoch:return None
    rounds=[]
    for line in (s for s in text.splitlines() if s.startswith('ap-package-round:')):
        m=re.fullmatch(r'ap-package-round: seq=(\d+) tsc=(\d+) c2=(\d+)',line)
        if not m:return None
        seq,tsc,c2=map(int,m.groups())
        if seq!=len(rounds)+1 or not 0<tsc<2**64 or not 0<=c2<=tsc:return None
        rounds.append(dict(seq=seq,tsc=tsc,c2=c2))
    if len(rounds)!=32:return None
    # The C2 bracket encloses the narrower existing BSP core/C3 bracket.
    for row,core in zip(rounds,coord['rows']):
        if row['tsc']<core['tsc'] or row['c2']+core['package']>row['tsc']:return None
    snapshots=[{},{}]
    for line in (s for s in text.splitlines() if s.startswith('ap-package-pci:')):
        m=re.fullmatch(r'ap-package-pci: phase=(\d+) bdf=(\d+) id=(\d+) class=(\d+) state=1 command=(\d+) flags=(\d+) pm=(\d+) link_cap=(\d+) link=(\d+) ltr_cap=(\d+) ltr=(\d+)',line)
        if not m:return None
        phase,bdf,identity,cls,cmd,flags,pm,lc,link,tc,ltr=map(int,m.groups())
        if phase not in (0,1) or bdf>=65536 or bdf in snapshots[phase] or identity>=2**32 or identity&65535 in (0,65535) or cls>=2**24 or any(v>=65536 for v in (cmd,pm,ltr)) or flags>7 or any(v>=2**32 for v in (lc,link,tc)):return None
        if (not flags&1 and pm) or (not flags&2 and (lc or link)) or (not flags&4 and (tc or ltr)):return None
        snapshots[phase][bdf]=dict(id=identity,**{'class':cls},command=cmd,flags=flags,pm=pm,link_cap=lc,link=link,ltr_cap=tc,ltr=ltr)
    if len(snapshots[0])!=count or snapshots[0].keys()!=snapshots[1].keys():return None
    devices=[]
    for bdf,start in snapshots[0].items():
        end=snapshots[1][bdf]
        if (start['id'],start['class'],start['flags'])!=(end['id'],end['class'],end['flags']):return None
        def decode(v):
            return dict(bus_master_enabled=bool(v['command']&4),power_state=v['pm']&3 if v['flags']&1 else None,
                        aspm_enabled=v['link']&3 if v['flags']&2 else None,aspm_supported=(v['link_cap']>>10)&3 if v['flags']&2 else None,
                        ltr_supported=bool(v['ltr_cap']&(1<<11)) if v['flags']&4 else None,ltr_enabled=bool(v['ltr']&(1<<10)) if v['flags']&4 else None)
        devices.append(dict(bdf=bdf,start=start,end=end,decoded_start=decode(start),decoded_end=decode(end)))
    total_tsc=sum(x['tsc'] for x in rounds);total_c2=sum(x['c2'] for x in rounds)
    if total_tsc>=2**64 or total_c2>=2**64:return None
    return dict(status='PASS',epoch=epoch,rounds=rounds,package_c2_observed=bool(total_c2),package_c2_ticks=total_c2,bracket_tsc=total_tsc,
                snapshot_us=[start_us,end_us],irtl_raw=a,irtl=irtl_decode(a),devices=devices,gpu_rc6='unknown',platform_permission='unknown',
                scope='Read-only context; C2 brackets are wider than BSP waits; no automatic causal blocker diagnosis, policy change or energy-saving acceptance')
