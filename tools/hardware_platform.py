"""Strict optional platform receipts; no inferred platform grant or RC6 policy."""
import re
from hardware_package import package_result

def latency_decode(raw):
    # PCIe LTR capability fields have value[9:0], scale[12:10], reserved[15:13].
    if raw & 0xe000 or ((raw>>10)&7) > 5:return None
    return (raw&1023)*(1<<(((raw>>10)&7)*5))

def platform_result(text):
    try:return _parse(text)
    except (ValueError,KeyError,OverflowError):return None

def _parse(text):
    package=package_result(text)
    if not package:return None
    h=[x for x in text.splitlines() if x.startswith('ap-platform:')]
    if len(h)!=1:return None
    m=re.fullmatch(r'ap-platform: v=1 fault=0 devices=(\d+) policy_writes=0 forcewake_writes=0 permission=unavailable reason=no-validated-grant-register completed=yes',h[0])
    if not m or int(m[1])!=len(package['devices']):return None
    gfx=[x for x in text.splitlines() if x.startswith('ap-platform-gfx:')]
    if len(gfx)!=1:return None
    m=re.fullmatch(r'ap-platform-gfx: bdf=16 id=170295430 base=(\d+) time_us=(\d+),(\d+) status=(\d+),(\d+) rc6=(\d+),(\d+) config=(\d+),(\d+) rc_control=unavailable',gfx[0])
    if not m:return None
    base,t0,t1,s0,s1,r0,r1,c0,c1=map(int,m.groups())
    if not base or base&0x3fffff or base>0xfffffff800000 or not 0<t0<t1<2**64 or c0!=c1 or c0>=2**64 or any(v>=0xffffffff for v in (s0,s1,r0,r1)):return None
    if not re.search(r"^ap-c3: .* config="+str(c0)+r" limit_us=20000 ",text,re.M):return None
    # The 32-bit RC6 counter wraps after about 91 minutes. Refuse ambiguous spans.
    elapsed=t1-t0
    if elapsed*1000>=2**32*1280:return None
    delta=(r1-r0)&0xffffffff
    if delta*1280>elapsed*1000+1280:return None
    # Snapshot calls are inside the matching package snapshot boundaries.
    if not package['snapshot_us'][0]<=t0<t1 or t1<package['snapshot_us'][1]:return None
    devices={v['bdf']:v for v in package['devices']};rows={}
    if 16 not in devices or devices[16]['start']['id']!=170295430:return None
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-ltr:')):
        m=re.fullmatch(r'ap-platform-ltr: bdf=(\d+) id=(\d+) state=(\d+),(\d+) offset=(\d+),(\d+) raw=(\d+),(\d+) headers=(\d+),(\d+)',line)
        if not m:return None
        bdf,ident,a,b,o0,o1,v0,v1,n0,n1=map(int,m.groups())
        if bdf in rows or bdf not in devices or ident!=devices[bdf]['start']['id'] or a!=b or o0!=o1:return None
        decoded=[]
        for state,off,raw,n in ((a,o0,v0,n0),(b,o1,v1,n1)):
            if state not in (1,2) or raw>=2**32 or n>960:return None
            if state==1:
                if not 0x100<=off<=0xff8 or off&3 or not n:return None
                decoded.append(dict(snoop_ns=latency_decode(raw&65535),nosnoop_ns=latency_decode(raw>>16)))
            else:
                if off or raw:return None
                decoded.append(None)
        rows[bdf]=dict(state=a,offset=o0,raw=[v0,v1],decoded=decoded,headers=[n0,n1])
    if rows.keys()!=devices.keys():return None
    return dict(status='PASS',graphics=dict(base=base,time_us=[t0,t1],status_raw=[s0,s1],rc_state=[{0:'RC0',2:'RC3',3:'RC6',4:'RC7'}.get(v&7,'unknown') for v in (s0,s1)],rc6_raw=[r0,r1],rc6_delta_ticks=delta,rc6_delta_ns=delta*1280,rc6_observed=bool(delta),rc_control='unavailable'),firmware_config=c0,permission='unavailable',permission_reason='no-validated-grant-register',latency=rows,scope='Paired read-only context, no causal blocker or policy inference; max LTR capability is not measured response latency or proof of a current request')
