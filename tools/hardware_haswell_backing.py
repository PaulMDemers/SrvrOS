"""Strict isolated Haswell vector receipts. Observations never grant ownership."""
import base64
import re
import struct
from haswell_backing_audit import plane_extent,ggc_fields,pte_fields,fence_fields,fence_candidates

FIELDS=('gpu rev sub mmio_lo mmio_hi ap_lo ap_hi cmd pm host ggc bdsm bgsm tolud gpu_ggc gpu_bdsm pch disparb '
        'control stride linoff surf offset source live control2 surf2 well0 well1 well2 well3 '
        'fb_count va pa width height pitch span mem_base mem_len mem_type pages entries bpp color classification contiguous mmio_safe firmware stolen_reserved').split()
META='<50Q16Q3Q4I'

def endpoint(values):
    e=dict(zip(FIELDS,values))
    if len(values)!=50 or any(x>0xffffffff for x in values[:31]):raise ValueError('endpoint width')
    if (e['gpu'],e['rev'],e['sub'],e['host'],e['pch'])!=(0x0a268086,0x03000009,0x011a106b,0x0a048086,0x9c438086):raise ValueError('identity')
    if e['mmio_hi'] or e['mmio_lo']&0x3fffff!=4 or not e['mmio_lo']&~15 or e['mmio_safe']!=1 or e['firmware']!=1 or not e['cmd']&2 or e['cmd']>65535 or e['pm']>65535 or e['pm']&3:raise ValueError('access')
    if any(e['well'+str(i)]==0xffffffff or not e['well'+str(i)]&(1<<30) for i in range(4)):raise ValueError('well')
    if e['contiguous']!=1 or not 1<=e['entries']<=4096 or not 0xffff800000000000<=e['va']<2**64-e['span'] or e['span']>4096*4096-(e['va']&4095) or e['pages']!=((e['va']&4095)+e['span']+4095)//4096:raise ValueError('mapping')
    if e['classification'] not in (1,2) or e['mem_type']!=(7 if e['classification']==1 else 1) or not e['mem_base']<=e['pa']<e['pa']+e['span']<=e['mem_base']+e['mem_len']<2**64 or e['stolen_reserved'] not in (0,1):raise ValueError('memory classification')
    sample=dict(state=1,reads=9,raw=values[18:27],framebuffer=values[31:38],bpp=e['bpp'],color=[(e['color']>>(8*i))&255 for i in range(7)],pci=[e['ap_lo'],e['ap_hi'],e['ap_lo'],e['ap_hi']])
    if e['color']>>56:raise ValueError('color padding')
    extent=plane_extent(sample);gc=ggc_fields(e['ggc']&65535)
    if e['gpu_ggc']&65535!=gc['raw'] or gc['ggms'] not in (1,2) or not 1<=gc['gms']<=16 or e['bdsm']&0xfff00000!=e['gpu_bdsm']&0xfff00000:raise ValueError('Haswell GGC')
    table=gc['table_bytes_candidate'];start=e['bdsm']&0xfff00000;end=start+gc['stolen_bytes_candidate'];bgsm=e['bgsm']&0xfff00000;top=e['tolud']&0xfff00000
    if not start or not bgsm or end>top or bgsm+table>top or (bgsm<end and start<bgsm+table) or (extent['offset']//4096+extent['pages'])*4>table:raise ValueError('stolen/table bounds')
    return e,extent,table,start,end


def canonical(data,size=None):
    b=base64.b64decode(data,validate=True)
    if (size is not None and len(b)!=size) or base64.b64encode(b).decode()!=data:raise ValueError('canonical bytes')
    return b


def backing_result(text):
    try:return _parse(text)
    except (ValueError,KeyError,IndexError,struct.error):return None


def _parse(text):
    lines=[x for x in text.splitlines() if x.startswith('hsw-backing:')]
    if len(lines)!=1:return None
    m=re.fullmatch(r'hsw-backing: v=1 status=([0-7]) count=(\d+) cap=1536 fences=16 upper=unknown swizzle=partial maps=(\d+) retained=([01]) offset=(\d+) bytes=(\d+) table=(\d+) stolen=(\d+),(\d+) limit_us=100000 writes=0 ownership=0',lines[0])
    if not m:return None
    status,count,maps,retained,offset,size,table,start,end=map(int,m.groups())
    if count>1536 or maps>10 or any(x>=2**64 for x in (offset,size,table,start,end)):return None
    metas={};vectors={}
    for line in text.splitlines():
        if not line.startswith(('hsw-backing-meta:','hsw-backing-ptes:')):continue
        m=re.fullmatch(r'hsw-backing-(meta|ptes): v=1 phase=([01]) data=([A-Za-z0-9+/]*={0,2})',line)
        if not m:return None
        kind,phase,data=m[1],int(m[2]),m[3];target=metas if kind=='meta' else vectors
        if phase in target:return None
        b=canonical(data,568 if kind=='meta' else None)
        if kind=='meta':target[phase]=list(struct.unpack(META,b))
        else:
            if len(b)%4 or len(b)>1536*4:return None
            target[phase]=list(struct.unpack('<'+'I'*(len(b)//4),b))
    if metas.keys()!={0,1} or vectors.keys()!={0,1}:return None
    complete=text.count('laptop-capture: completed=yes\n')==1 and 'laptop-capture: completed=NO' not in text
    if status!=1:
        return dict(status='REFUSED' if status==2 else 'FAILED',source_status=status,source_completed=False,hardware_authority=False,ownership=False,maps=maps,retained=bool(retained),observed_count=count,raw_metadata=[metas[0],metas[1]],partial_ptes=[vectors[0],vectors[1]],scope='Incomplete/refused readout, not backing acceptance')
    if retained or not count or not complete:return None
    samples=[]
    for phase in (0,1):
        v=metas[phase];raw=vectors[phase];e,extent,tt,ss,se=endpoint(v[:50])
        fences=v[50:66];begin,finish,digest=v[66:69];reads,n,present,stolen=v[69:73]
        if (extent['pages'],extent['offset'],extent['span'],tt,ss,se)!=(count,offset,size,table,start,end) or n!=count or len(raw)!=count or reads!=count+96:return None
        if not 0<begin<finish<2**64 or finish-begin>100000:return None
        if any((f&0xffffffff)==0xffffffff or f>>32==0xffffffff for f in fences):return None
        for f in fences:fence_fields(f) # Raw field preservation, no unique/upper-fence assertion.
        h=14695981039346656037
        for word in raw:
            for byte in word.to_bytes(4,'little'):h=((h^byte)*1099511628211)&((1<<64)-1)
        pages=[pte_fields(p)['address_candidate'] for p in raw]
        if h!=digest or present!=sum(p is not None for p in pages) or stolen!=sum(p is not None and start<=p<end for p in pages):return None
        if samples and (metas[0][:66]!=v[:66] or vectors[0]!=raw or begin<samples[0]['pm_us'][1] or begin-samples[0]['pm_us'][1]>100000):return None
        samples.append(dict(endpoint=e,raw_endpoint=v[:50],fences=fences,pm_us=[begin,finish],hash=digest,window_reads=reads,present=present,stolen_candidates=stolen,ptes=raw,address_candidates=pages))
    ptepages=((offset//4096*4&4095)+count*4+4095)//4096
    if maps!=2*(ptepages+2):return None
    candidates=fence_candidates(samples[0]['fences'],extent,e['pitch'])
    return dict(all_present=all(s['present']==count for s in samples),all_stolen_candidates=all(s['stolen_candidates']==count for s in samples),stolen_range_reserved=bool(e['stolen_reserved']),lower_fence_candidates=candidates['matches'],display_swizzle_bit=bool(e['disparb']&(1<<13)),status='OBSERVED',source_status=1,source_completed=True,count=count,maps=maps,retained=False,offset=offset,bytes=size,table_bytes=table,stolen_bounds=[start,end],samples=samples,upper_fences='unknown',swizzle='partial',unique_fence_proven=False,hardware_authority=False,ownership=False,acceptance=False,scope='Exact Haswell full GGTT vectors and documented lower fences only; upper fences/full swizzle/firmware exclusion/pinning unknown; original RC6 acceptance separate')
