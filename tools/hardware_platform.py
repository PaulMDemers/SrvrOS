"""Strict optional platform receipts; no inferred platform grant or RC6 policy."""
import re
from hardware_package import package_result
from cpu_capture_format import capture_input

def latency_decode(raw):
    # PCIe LTR capability fields have value[9:0], scale[12:10], reserved[15:13].
    if raw & 0xe000 or ((raw>>10)&7) > 5:return None
    return (raw&1023)*(1<<(((raw>>10)&7)*5))

def platform_timing_result(text):
    """Diagnostic evidence even on a failed capture; never an acceptance result.

    Preserve the original snapshot-clock bound. Tight PM/TSC brackets explain
    placement uncertainty but do not supply a new tolerance or calibrated ratio.
    """
    gfx=[x for x in text.splitlines() if x.startswith('ap-platform-gfx:')]
    if len(gfx)!=1:return None
    m=re.fullmatch(r'ap-platform-gfx: bdf=16 id=170295430 base=(\d+) time_us=(\d+),(\d+) status=(\d+),(\d+) rc6=(\d+),(\d+) config=(\d+),(\d+) rc_control=unavailable',gfx[0])
    if not m:return None
    base,t0,t1,s0,s1,r0,r1,c0,c1=map(int,m.groups())
    if not 0<t0<t1<2**64 or any(v>=0xffffffff for v in (r0,r1)):return None
    rows={}
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-time:')):
        m=re.fullmatch(r'ap-platform-time: phase=([01]) pm_us=(\d+),(\d+) tsc=(\d+),(\d+) nominal_mhz=(\d+) reference=acpi-pm',line)
        if not m:return None
        phase,a,b,c,d,mhz=map(int,m.groups())
        if phase in rows or not 0<a<=b<2**64 or not 0<c<d<2**64 or not 0<mhz<=6400:return None
        rows[phase]=dict(pm_us=[a,b],tsc=[c,d],nominal_mhz=mhz)
    if rows.keys()!={0,1}:return None
    a,b=rows[0],rows[1]
    if not t0<=a['pm_us'][0]<=a['pm_us'][1]<t1<=b['pm_us'][0] or a['tsc'][1]>=b['tsc'][0] or a['nominal_mhz']!=b['nominal_mhz']:return None
    pm_min=b['pm_us'][0]-a['pm_us'][1];pm_max=b['pm_us'][1]-a['pm_us'][0]
    if max(pm_max,t1-t0)*1000>=2**32*1280:return None
    tsc_min=b['tsc'][0]-a['tsc'][1];tsc_max=b['tsc'][1]-a['tsc'][0]
    nominal_ns=((r1-r0)&0xffffffff)*1280
    return dict(status='OBSERVED',acceptance=False,samples=[a,b],rc6_nominal_ns=nominal_ns,
        snapshot_elapsed_us=t1-t0,original_bound_ok=nominal_ns<=(t1-t0)*1000+1280,
        original_excess_ns=max(0,nominal_ns-(t1-t0)*1000-1280),
        pm_elapsed_us_bounds=[pm_min,pm_max],tsc_elapsed_ticks_bounds=[tsc_min,tsc_max],
        tsc_ticks_per_us_bounds=[tsc_min/pm_max,tsc_max/pm_min],
        bracket_excess_ns=max(0,nominal_ns-pm_max*1000-1280),
        scope='Raw cross-clock diagnostics only; nominal MHz is not a TSC calibration; no new tolerance')

@capture_input
def platform_series_result(text):
    """Retained counter-update evidence, including mismatches; never a pass."""
    try:return _series(text)
    except (ValueError,KeyError,OverflowError):return None

def _series(text):
    timing=platform_timing_result(text)
    if not timing:return None
    headers=re.findall(r'^ap-platform-series: v=1 count=(\d+) cap=12 checkpoints=(\d+) stride=2 overflow=0 completed=yes$',text,re.M)
    if len(headers)!=1 or sum(x.startswith('ap-platform-series:') for x in text.splitlines())!=1:return None
    count,checkpoints=map(int,headers[0])
    if not 1<=checkpoints<=24 or count!=(checkpoints+1)//2:return None
    retained=re.findall(r'^cpu-load-power: retained=(\d+) overflow=0$',text,re.M)
    if retained!=[str(checkpoints)]:return None
    samples=[]
    for line in text.splitlines():
        if line.startswith('cpu-load-power: power_state='):
            m=re.search(r'\bsample_us=(\d+) ',line)
            if not m:return None
            samples.append(int(m[1]))
    if len(samples)!=checkpoints:return None
    samples=samples[::2]
    rows=[]
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-point:')):
        m=re.fullmatch(r'ap-platform-point: seq=(\d+) sample_us=(\d+) pm_us=(\d+),(\d+) tsc=(\d+),(\d+) rc6=(\d+),(\d+) status=(\d+),(\d+)',line)
        if not m:return None
        seq,us,p0,p1,t0,t1,r0,r1,s0,s1=map(int,m.groups())
        if seq!=len(rows)+1 or seq>count or us!=samples[seq-1]:return None
        if not 0<us<=p0<=p1<2**64 or not 0<t0<t1<2**64 or any(v>=0xffffffff for v in (r0,r1,s0,s1)):return None
        if rows and us-rows[-1]['sample_us']<10000000:return None
        rows.append(dict(seq=seq,sample_us=us,pm_us=[p0,p1],tsc=[t0,t1],rc6=[r0,r1],status_raw=[s0,s1],pair_delta_ticks=(r1-r0)&0xffffffff))
    if len(rows)!=count:return None
    gfx=next(x for x in text.splitlines() if x.startswith('ap-platform-gfx:'))
    raw=list(map(int,re.search(r' rc6=(\d+),(\d+) ',gfx).groups()))
    ends=[dict(timing['samples'][i],rc6=[raw[i],raw[i]],seq=0 if i==0 else count+1) for i in (0,1)]
    points=[ends[0],*rows,ends[1]];intervals=[];total=sum(x['pair_delta_ticks'] for x in rows)
    for a,b in zip(points,points[1:]):
        low=b['pm_us'][0]-a['pm_us'][1];high=b['pm_us'][1]-a['pm_us'][0]
        if low<=0 or b['tsc'][0]<=a['tsc'][1]:return None
        delta=(b['rc6'][0]-a['rc6'][1])&0xffffffff;total+=delta
        nominal=delta*1280
        intervals.append(dict(start=a['seq'],end=b['seq'],pm_elapsed_us_bounds=[low,high],
            tsc_elapsed_ticks_bounds=[b['tsc'][0]-a['tsc'][1],b['tsc'][1]-a['tsc'][0]],
            rc6_delta_ticks=delta,rc6_nominal_ns=nominal,
            nominal_excess_ns=max(0,nominal-high*1000-1280)))
    return dict(status='OBSERVED',acceptance=False,rows=rows,intervals=intervals,
        continuity_ok=total==((raw[1]-raw[0])&0xffffffff),original_bound_ok=timing['original_bound_ok'],
        scope='Sparse counter-update observations; no inferred clock calibration, update cadence or new acceptance tolerance')

def platform_result(text):
    try:return _parse(text)
    except (ValueError,KeyError,OverflowError):return None

def platform_display_result(text):
    """Endpoint request/status observations, independent of RC6 acceptance."""
    try:return _display(text)
    except (ValueError,KeyError,OverflowError):return None

def _display(text):
    timing=platform_timing_result(text)
    if not timing:return None
    gfx=next(x for x in text.splitlines() if x.startswith('ap-platform-gfx:'))
    base=int(re.search(r' base=(\d+) ',gfx)[1])
    if not base or base&0x3fffff or base>0xfffffff800000:return None
    headers=[x for x in text.splitlines() if x.startswith('ap-platform:')]
    if len(headers)!=1:return None
    h=re.fullmatch(r'ap-platform: v=(?:4|5|6|7) fault=(\d+) devices=(\d+) policy_writes=0 forcewake_writes=0 permission=unavailable reason=no-validated-grant-register completed=(yes|NO)',headers[0])
    if not h or int(h[1])>=2**32 or not 1<=int(h[2])<=128:return None
    rows={}
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-well:')):
        m=re.fullmatch(r'ap-platform-well: phase=([01]) pm_us=(\d+),(\d+) ctl=(\d+),(\d+),(\d+),(\d+)',line)
        if not m:return None
        phase,low,high,*values=map(int,m.groups())
        if phase in rows or not 0<low<=high<2**64 or any(v>=0xffffffff for v in values):return None
        if low<timing['samples'][phase]['pm_us'][1]:return None
        rows[phase]=dict(pm_us=[low,high],raw=values,
            controls={name:dict(request=bool(v&(1<<31)),state=bool(v&(1<<30)))
                      for name,v in zip(('bios','driver','kvm','debug'),values)})
    if rows.keys()!={0,1} or rows[0]['pm_us'][1]>=timing['samples'][1]['pm_us'][0]:return None
    return dict(status='OBSERVED',acceptance=False,samples=[rows[0],rows[1]],
        source_fault=int(h[1]),source_completed=h[3]=='yes',
        scope='Non-atomic endpoint request/status reads only; no forced wake, power transition, platform grant or causal C3 blocker inference')

def _parse(text):
    package=package_result(text)
    if not package:return None
    h=[x for x in text.splitlines() if x.startswith('ap-platform:')]
    if len(h)!=1:return None
    m=re.fullmatch(r'ap-platform: v=(1|2|3|4|5|6|7) fault=0 devices=(\d+) policy_writes=0 forcewake_writes=0 permission=unavailable reason=no-validated-grant-register completed=yes',h[0])
    if not m or int(m[2])!=len(package['devices']):return None
    version=int(m[1]);timing=platform_timing_result(text)
    if version>=2 and not timing:return None
    series=platform_series_result(text) if version>=3 else None
    if version>=3 and (not series or not series['continuity_ok']):return None
    display=platform_display_result(text) if version>=4 else None
    if version>=4 and not display:return None
    resources=platform_resources_result(text) if version>=5 else None
    if version>=5 and (not resources or resources['status']!='OBSERVED' or
                      resources['source_fault'] or not resources['source_completed']):return None
    dependencies=platform_dependencies_result(text) if version>=6 else None
    hda=platform_hda_result(text) if version>=7 else None
    if version>=7 and (not hda or hda['status']!='OBSERVED'):return None
    if version>=6 and (not dependencies or dependencies['status']!='OBSERVED' or dependencies['source_fault'] or not dependencies['source_completed']):return None
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
    return dict(status='PASS',timing=timing,series=series,display_well=display,resources=resources,dependencies=dependencies,hda=hda,graphics=dict(base=base,time_us=[t0,t1],status_raw=[s0,s1],rc_state=[{0:'RC0',2:'RC3',3:'RC6',4:'RC7'}.get(v&7,'unknown') for v in (s0,s1)],rc6_raw=[r0,r1],rc6_delta_ticks=delta,rc6_delta_ns=delta*1280,rc6_observed=bool(delta),rc_control='unavailable'),firmware_config=c0,permission='unavailable',permission_reason='no-validated-grant-register',latency=rows,scope='Paired read-only context, no causal blocker or policy inference; max LTR capability is not measured response latency or proof of a current request')


RESOURCE_OFFSETS = (0x70008,0x71008,0x72008,0x7f008,0x60400,0x61400,0x62400,0x6f400,
                    0x70180,0x71180,0x72180,0x70280,0x71280,0x72280,0x70080,0x71080,0x72080,
                    0x68080,0x68880,0x69080,0x64000,0x64100,0x64200,0x64300,0x64400,0x41000)

def platform_resources_result(text):
    """Non-atomic raw control inventory; no ownership/power-policy acceptance."""
    try:return _resources(text)
    except (ValueError,KeyError,OverflowError,IndexError,StopIteration):return None

def _resources(text):
    display=platform_display_result(text)
    if not display or not re.search(r'^ap-platform: v=(?:5|6|7) ',text,re.M):return None
    rows={}
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-resources:')):
        m=re.fullmatch(r'ap-platform-resources: v=1 phase=([01]) state=([123]) pm_us=(\d+),(\d+) reads=(\d+) count=26 before=(\d+),(\d+),(\d+),(\d+) after=(\d+),(\d+),(\d+),(\d+)',line)
        if not m:return None
        phase,state,a,b,n,*ctl=map(int,m.groups())
        if phase!=len(rows) or not 0<a<=b<2**64 or not 0<=n<=26 or any(v>=0xffffffff for v in ctl):return None
        if a<display['samples'][phase]['pm_us'][1]:return None
        if state==1 and (n!=26 or ctl[:4]!=ctl[4:] or not all(v&(1<<30) for v in ctl)):return None
        if state==2 and (n or any(ctl[4:]) or all(v&(1<<30) for v in ctl[:4])):return None
        if state==3:return None # Invalid capture remains raw failure evidence.
        rows[phase]=dict(state=state,pm_us=[a,b],before=ctl[:4],after=ctl[4:],raw=[],read_count=n)
    if rows.keys()!={0,1} or rows[0]['pm_us'][1]>=display['samples'][1]['pm_us'][0]:return None
    raw_seen=set()
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-resource:')):
        m=re.fullmatch(r'ap-platform-resource: phase=([01]) count=26 raw=(\d+(?:,\d+){25})',line)
        if not m:return None
        phase=int(m[1]);values=list(map(int,m[2].split(',')))
        if phase!=len(raw_seen) or any(v>=0xffffffff for v in values):return None
        raw_seen.add(phase);rows[phase]['raw']=values
    if raw_seen!={0,1}:return None
    for r in rows.values():
        if r['state']==2 and any(r['raw']):return None
        if r['state']==1:
            v=r['raw']
            r['decoded']=dict(transcoders=[dict(enable=bool(x&(1<<31)),state=bool(x&(1<<30))) for x in v[:4]],
                ddi_function=[dict(enable=bool(x&(1<<31)),port_select=((x>>28)&7) if i<3 else None,mode=(x>>24)&7) for i,x in enumerate(v[4:8])],
                edp_input_select=(v[7]>>12)&7,
                primary_enabled=[bool(x&(1<<31)) for x in v[8:11]],
                sprite_enabled=[bool(x&(1<<31)) for x in v[11:14]],
                cursor_modes=[x&0x27 for x in v[14:17]],
                fitters=[dict(enable=bool(x&(1<<31)),pipe_select=(x>>29)&3) for x in v[17:20]],
                ddi_buffer_enabled=[bool(x&(1<<31)) for x in v[20:25]],vga_disabled=bool(v[25]&(1<<31)))
        else:r['decoded']=None
    return dict(status='OBSERVED' if all(r['state']==1 for r in rows.values()) else 'UNAVAILABLE',acceptance=False,
        samples=[rows[0],rows[1]],source_fault=display['source_fault'],source_completed=display['source_completed'],
        scope='Fixed read-only non-atomic endpoint controls; off-domain unknowns retained; no audio/interrupt inventory, ownership transfer or causal blocker inference')


def platform_dependencies_result(text):
    """Read-only graphics-audio/IRQ context, not ownership or DMA retirement."""
    try:return _dependencies(text)
    except (ValueError,KeyError,IndexError,OverflowError,StopIteration):return None

def _dependencies(text):
    resource=platform_resources_result(text)
    if not resource or not re.search(r'^ap-platform: v=(?:6|7) ',text,re.M):return None
    rows=[]
    for line in (x for x in text.splitlines() if x.startswith('ap-platform-deps:')):
        m=re.fullmatch(r'ap-platform-deps: v=1 phase=([01]) state=([123]) pm_us=(\d+),(\d+) reads=(\d+) count=8 pch=(\d+),(\d+) before=(\d+),(\d+),(\d+),(\d+) after=(\d+),(\d+),(\d+),(\d+) raw=(\d+(?:,\d+){7})',line)
        if not m:return None
        phase,state,a,b,n,p0,p1,*ctl=map(int,m.groups()[:-1]);raw=list(map(int,m.groups()[-1].split(',')))
        if phase!=len(rows) or not 0<a<=b<2**64 or n>8 or any(v>=0xffffffff for v in ctl) or any(v>=2**32 for v in raw):return None
        if a<resource['samples'][phase]['pm_us'][1]:return None
        if state==3:return None
        if p0!=0x9c438086:return None
        if state==1 and (n!=8 or p1!=p0 or ctl[:4]!=ctl[4:] or not all(v&(1<<30) for v in ctl) or any(v==0xffffffff for v in raw[:4])):return None
        if state==2 and (n or p1 or any(ctl[4:]) or any(raw) or all(v&(1<<30) for v in ctl[:4])):return None
        decoded=None
        if state==1:
            pin=raw[0]
            decoded=dict(audio=[dict(eld_valid=bool(pin&(1<<(4*i))),cp_ready=bool(pin&(2<<(4*i))),output_enable=bool(pin&(4<<(4*i))),inactive=bool(pin&(8<<(4*i)))) for i in range(3)],audio_config=raw[1:4],
                         cpu_irq=dict(mask=raw[4],enable=raw[5],enable_and_not_mask=raw[5]&(~raw[4]&0xffffffff)),
                         pch_irq=dict(mask=raw[6],enable=raw[7],enable_and_not_mask=raw[7]&(~raw[6]&0xffffffff)))
        rows.append(dict(state=state,pm_us=[a,b],pch=[p0,p1],before=ctl[:4],after=ctl[4:],raw=raw,read_count=n,decoded=decoded))
    if len(rows)!=2 or rows[0]['pm_us'][1]>=resource['samples'][1]['pm_us'][0]:return None
    return dict(status='OBSERVED' if all(r['state']==1 for r in rows) else 'UNAVAILABLE',acceptance=False,samples=rows,
                source_fault=resource['source_fault'],source_completed=resource['source_completed'],
                scope='Non-atomic control readbacks only; no HDA/IRQ owner, DMA retirement, platform permission or causal power inference')

def platform_hda_result(text):
    """Exact display-HDA control observations; never DMA retirement/ownership."""
    try:return _hda(text)
    except (ValueError,KeyError,IndexError,OverflowError):return None

def _hda(text):
    deps=platform_dependencies_result(text)
    if not deps or not re.search(r'^ap-platform: v=7 ',text,re.M):return None
    controls=[x for x in text.splitlines() if x.startswith('ap-platform-hda:')]
    streams=[x for x in text.splitlines() if x.startswith('ap-platform-hda-sd:')]
    if len(controls)!=2 or len(streams)!=2:return None
    rows=[]
    for phase,(line,sdline) in enumerate(zip(controls,streams)):
        m=re.fullmatch(r'ap-platform-hda: v=1 phase=([01]) state=([1234]) pm_us=(\d+),(\d+) reads=(\d+) streams=(\d+) base=(\d+) pci=(\d+),(\d+),(\d+),(\d+) before=(\d+),(\d+),(\d+),(\d+) after=(\d+),(\d+),(\d+),(\d+) raw=(\d+(?:,\d+){7})',line)
        t=re.fullmatch(r'ap-platform-hda-sd: phase=([01]) raw=(\d+(?:,\d+){29})',sdline)
        if not m or not t:return None
        ph,state,a,b,reads,n,base,*gate=map(int,m.groups()[:-1]);raw=list(map(int,m.groups()[-1].split(',')));sd=list(map(int,t[2].split(',')))
        if text.index(sdline)<text.index(line) or (phase==0 and text.index(sdline)>text.index(controls[1])):return None
        if ph!=phase or int(t[1])!=phase or state==3 or not 0<a<=b<2**64 or n>30:return None
        if a<deps['samples'][phase]['pm_us'][1] or any(v>=0xffffffff for v in gate) or any(v>=65536 for v in gate[:4]):return None
        if not base or base&0x3fff or base>0x000fffffffffc000:return None
        if any(v>=2**w for v,w in zip(raw,[16,32,32,8,8,32,16,32])) or any(v>=0xffffff for v in sd):return None
        if state==2:
            if reads or n or any(raw) or any(sd) or any(gate[2:4]) or any(gate[8:]):return None
            if not (gate[1]&3 or not gate[0]&2 or not all(v&(1<<30) for v in gate[4:8])):return None
            decoded=None
        else:
            if gate[:2]!=gate[2:4] or not gate[0]&2 or gate[1]&3 or gate[4:8]!=gate[8:] or not all(v&(1<<30) for v in gate[4:]):return None
            gcap,gctl=raw[:2];total=((gcap>>12)&15)+((gcap>>8)&15)+((gcap>>3)&31)
            if gcap==65535 or gctl==0xffffffff or not 1<=total<=30 or ((gcap>>1)&3)==3 or raw[6:]!=raw[:2]:return None
            if state==4:
                if gctl&1 or reads!=4 or n or any(raw[2:6]) or any(sd):return None
                decoded=dict(controller_reset=True,other_controls='unknown')
            else:
                if not gctl&1 or n!=total or reads!=8+2*n or any(sd[n:]) or any(raw[i]==v for i,v in [(2,0xffffffff),(3,255),(4,255),(5,0xffffffff)]):return None
                decoded=dict(controller_reset=False,interrupt_control=raw[2],corb_dma=bool(raw[3]&2),rirb_dma=bool(raw[4]&2),position_buffer_enabled=bool(raw[5]&1),streams=[dict(run=bool(v&2),reset=bool(v&1),tag=(v>>20)&15) for v in sd[:n]])
        rows.append(dict(state=state,pm_us=[a,b],reads=reads,streams=n,base=base,pci=gate[:4],before=gate[4:8],after=gate[8:],raw=raw,stream_raw=sd,decoded=decoded))
    if rows[0]['pm_us'][1]>=deps['samples'][1]['pm_us'][0]:return None
    return dict(status='OBSERVED' if all(x['state']==1 for x in rows) else 'UNAVAILABLE',acceptance=False,samples=rows,source_fault=deps['source_fault'],source_completed=deps['source_completed'],scope='Non-atomic exact display-HDA controls only; reset/off controls unknown, no codec/link readiness, DMA retirement, ownership or power grant')
